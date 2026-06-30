#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "ZW101.h"
#include "Key.h"
#include "LED.h"
#include "Servo.h" // 注入新舵机模块头文件

// 运行模式定义
typedef enum {
    MODE_VERIFY,  // 检索验证模式 (默认模式)
    MODE_ENROLL,  // 注册录入模式
    MODE_EMPTY    // 清空指纹库模式
} SystemMode;

// 交互辅助全局变量
uint16_t EnrollID = 1; // 默认需要录入的ID
SystemMode CurrentMode = MODE_VERIFY;

/* 菜单 UI 显示更新 (加入中断安全锁，防止刷新时被触摸中断打断导致屏幕死锁) */
void UI_ShowMenu(void)
{
    NVIC_DisableIRQ(EXTI4_IRQn);  // 临时关闭 PA4 触摸中断
    NVIC_DisableIRQ(USART2_IRQn); // 临时关闭 USART2 串口接收中断，防止乱码
    
    OLED_Clear();
    OLED_ShowString(1, 1, "---ZW101 MENU---");
    
    if (CurrentMode == MODE_VERIFY)
    {
        OLED_ShowString(2, 1,">1.Verify Mode");
        OLED_ShowString(3, 1," 2.Enroll ID:");
        OLED_ShowNum(3, 14, EnrollID, 2);
        OLED_ShowString(4, 1," 3.Clear Library");
    }
    else if (CurrentMode == MODE_ENROLL)
    {
        OLED_ShowString(2, 1," 1.Verify Mode");
        OLED_ShowString(3, 1,">2.Enroll ID:");
        OLED_ShowNum(3, 14, EnrollID, 2);
        OLED_ShowString(4, 1," 3.Clear Library");
    }
    else if (CurrentMode == MODE_EMPTY)
    {
        OLED_ShowString(2, 1," 1.Verify Mode");
        OLED_ShowString(3, 1," 2.Enroll ID:");
        OLED_ShowNum(3, 14, EnrollID, 2);
        OLED_ShowString(4, 1,">3.Clear Library");
    }
    
    NVIC_EnableIRQ(USART2_IRQn);  // 刷新完毕，重新开启串口接收中断
    NVIC_EnableIRQ(EXTI4_IRQn);   // 刷新完毕，重新开启触摸中断
}

/* 核心录入控制 (交互式，带安全锁与按键消抖) */
void Do_Enroll(void)
{
    uint8_t status;
    uint8_t cancelFlag = 0;
    
    NVIC_DisableIRQ(EXTI4_IRQn); // 临时关闭中断
    OLED_Clear();
    OLED_ShowString(1, 1, "--- Enroll ID ---");
    OLED_ShowString(2, 1, "Target ID: ");
    OLED_ShowNum(2, 12, EnrollID, 2);
    OLED_ShowString(3, 1, "Place Finger... ");
    OLED_ShowString(4, 1, "K3 to Cancel");
    NVIC_EnableIRQ(EXTI4_IRQn);  // 重新开启中断
    
    // 1. 等待按压手指 (检测是否有按键3按下可退出，避免卡死)
    while (1)
    {
        status = ZW101_GetImage();
        if (status == 0x00) // 成功获取图像
        {
            break;
        }
        
        // 若按键3按下，退出当前录入操作
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0)
        {
            cancelFlag = 1;
            break;
        }
        Delay_ms(100);
    }
    
    if (cancelFlag)
    {
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "Enroll Cancelled");
        Delay_ms(1500);
        NVIC_EnableIRQ(EXTI4_IRQn);
        return;
    }
    
    // 生成特征码1
    status = ZW101_GenChar(1);
    if (status != 0x00)
    {
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "GenChar 1 Fail");
        OLED_ShowHexNum(2, 1, status, 2);
        Delay_ms(2000);
        NVIC_EnableIRQ(EXTI4_IRQn);
        return;
    }
    
    // 2. 提示抬起手指
    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "Lift Finger...");
    LED_Correct_ON(); // 第一次读取成功提示
    Delay_ms(500);
    LED_Correct_OFF();
    NVIC_EnableIRQ(EXTI4_IRQn);
    
    while (ZW101_GetImage() == 0x00)
    {
        Delay_ms(100); // 循环等待抬起手指
    }
    
    // 3. 再次按压手指
    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "Place Finger 2nd");
    OLED_ShowString(2, 1, "Press same finger");
    NVIC_EnableIRQ(EXTI4_IRQn);
    
    while (1)
    {
        status = ZW101_GetImage();
        if (status == 0x00)
        {
            break;
        }
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0)
        {
            cancelFlag = 1;
            break;
        }
        Delay_ms(100);
    }
    
    if (cancelFlag)
    {
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "Enroll Cancelled");
        Delay_ms(1500);
        NVIC_EnableIRQ(EXTI4_IRQn);
        return;
    }
    
    // 生成特征码2
    status = ZW101_GenChar(2);
    if (status != 0x00)
    {
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "GenChar 2 Fail");
        OLED_ShowHexNum(2, 1, status, 2);
        Delay_ms(2000);
        NVIC_EnableIRQ(EXTI4_IRQn);
        return;
    }
    
    // 4. 合并并保存
    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "Merging...");
    NVIC_EnableIRQ(EXTI4_IRQn);
    
    status = ZW101_RegModel();
    if (status == 0x00)
    {
        status = ZW101_StoreChar(EnrollID);
        if (status == 0x00)
        {
            NVIC_DisableIRQ(EXTI4_IRQn);
            OLED_Clear();
            OLED_ShowString(1, 1, "Enroll Success!");
            OLED_ShowString(2, 1, "ID registered:");
            OLED_ShowNum(2, 15, EnrollID, 2);
            NVIC_EnableIRQ(EXTI4_IRQn);
            
            // 闪烁三次指示灯表示成功
            for (int i = 0; i < 3; i++)
            {
                LED_Correct_ON();
                Delay_ms(200);
                LED_Correct_OFF();
                Delay_ms(100);
            }
            
            // 自动累加ID编号，准备下一次快速录入
            EnrollID++;
            if (EnrollID > 99) EnrollID = 1;
        }
        else
        {
            NVIC_DisableIRQ(EXTI4_IRQn);
            OLED_Clear();
            OLED_ShowString(1, 1, "Store Failed");
            OLED_ShowHexNum(2, 1, status, 2);
            NVIC_EnableIRQ(EXTI4_IRQn);
            LED_Incorrect_ON();
            Delay_ms(2000);
            LED_Incorrect_OFF();
        }
    }
    else
    {
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "Merge Failed");
        OLED_ShowString(2, 1, "Are they same?");
        NVIC_EnableIRQ(EXTI4_IRQn);
        LED_Incorrect_ON();
        Delay_ms(2000);
        LED_Incorrect_OFF();
    }
}

/* 核心比对验证模式 (检索模式) */
void Do_Verify(void)
{
    uint8_t status;
    uint16_t matchedID = 0;
    uint16_t score = 0;
    
    NVIC_DisableIRQ(EXTI4_IRQn); // 刷新前关闭触摸中断，严防I2C总线死锁
    OLED_Clear();
    OLED_ShowString(1, 1, "[VERIFY MODE]");
    OLED_ShowString(2, 1, "Place Finger...");
    OLED_ShowString(4, 1, "Press K3 to Menu");
    NVIC_EnableIRQ(EXTI4_IRQn);  // 重新使能中断
    
    while (1)
    {
        // 持续获取图像
        status = ZW101_GetImage();
        if (status == 0x00) // 获取到指纹
        {
            NVIC_DisableIRQ(EXTI4_IRQn);
            OLED_Clear();
            OLED_ShowString(1, 1, "Processing...");
            NVIC_EnableIRQ(EXTI4_IRQn);
            
            // 生成特征码在Buffer1中
            status = ZW101_GenChar(1);
            if (status == 0x00)
            {
                // 全库检索匹配
                status = ZW101_Search(&matchedID, &score);
                if (status == 0x00) // 检索到了对应指纹
                {
                    NVIC_DisableIRQ(EXTI4_IRQn);
                    OLED_Clear();
                    OLED_ShowString(1, 1, "Verify Success!");
                    OLED_ShowString(2, 1, "ID matched: ");
                    OLED_ShowNum(2, 13, matchedID, 2);
                    OLED_ShowString(3, 1, "Score: ");
                    OLED_ShowNum(3, 8, score, 3);
                    OLED_ShowString(4, 1, "[Unlocking...]"); // OLED提示解锁开门
                    NVIC_EnableIRQ(EXTI4_IRQn);
                    
                    // 正确指纹，亮 A1 灯一秒钟
                    LED_Correct_ON();
                    LED_Incorrect_OFF();
                    
                    // ============================================
                    // 新增：比对成功，驱动 MG996R 旋转到 80 度开门
                    // ============================================
                    Servo_SetAngle(80.0f); 
                    
                    Delay_ms(2000); // 维持开启开门状态 2 秒钟
                    
                    // ============================================
                    // 新增：自动关门复位到 0 度
                    // ============================================
                    Servo_SetAngle(0.0f);
                    
                    LED_Correct_OFF();
                }
                else // 指纹不匹配
                {
                    NVIC_DisableIRQ(EXTI4_IRQn);
                    OLED_Clear();
                    OLED_ShowString(1, 1, "Verify Failed!");
                    OLED_ShowString(2, 1, "Finger Not Found");
                    OLED_ShowString(3, 1, "Error Code: ");
                    OLED_ShowHexNum(3, 13, status, 2);
                    NVIC_EnableIRQ(EXTI4_IRQn);
                    
                    // 错误指纹，亮 A0 灯一秒钟
                    LED_Incorrect_ON();
                    LED_Correct_OFF();
                    Delay_ms(1000);
                    LED_Incorrect_OFF();
                }
            }
            else
            {
                NVIC_DisableIRQ(EXTI4_IRQn);
                OLED_Clear();
                OLED_ShowString(1, 1, "Bad Finger Print");
                OLED_ShowString(2, 1, "Try again");
                NVIC_EnableIRQ(EXTI4_IRQn);
                
                LED_Incorrect_ON();
                Delay_ms(800);
                LED_Incorrect_OFF();
            }
            
            // 结果显示完毕后提示重新放置
            Delay_ms(1000);
            NVIC_DisableIRQ(EXTI4_IRQn);
            OLED_Clear();
            OLED_ShowString(1, 1, "[VERIFY MODE]");
            OLED_ShowString(2, 1, "Place Finger...");
            OLED_ShowString(4, 1, "Press K3 to Menu");
            NVIC_EnableIRQ(EXTI4_IRQn);
        }
        
        // 若按键3按下，退出检索模式，返回菜单
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0)
        {
            Delay_ms(20); // 简易按键消抖
            while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0);
            Delay_ms(20);
            break;
        }
    }
}

/* 一键格式化清空指纹库 */
void Do_EmptyLibrary(void)
{
    uint8_t key;
    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "?? WARNING ??");
    OLED_ShowString(2, 1, "Are you sure?");
    OLED_ShowString(3, 1, "K1:Confirm");
    OLED_ShowString(4, 1, "K3:Cancel");
    NVIC_EnableIRQ(EXTI4_IRQn);
    
    while (1)
    {
        key = Key_GetNum();
        if (key == 1) // K1按下，执行清空
        {
            NVIC_DisableIRQ(EXTI4_IRQn);
            OLED_Clear();
            OLED_ShowString(1, 1, "Clearing Flash...");
            NVIC_EnableIRQ(EXTI4_IRQn);
            
            uint8_t status = ZW101_EmptyLibrary();
            if (status == 0x00)
            {
                NVIC_DisableIRQ(EXTI4_IRQn);
                OLED_Clear();
                OLED_ShowString(1, 1, "Clear Success!");
                NVIC_EnableIRQ(EXTI4_IRQn);
                
                // 红绿双灯闪烁提示完成
                for (int i = 0; i < 4; i++)
                {
                    GPIO_ResetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1);
                    Delay_ms(150);
                    GPIO_SetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1);
                    Delay_ms(150);
                }
            }
            else
            {
                NVIC_DisableIRQ(EXTI4_IRQn);
                OLED_Clear();
                OLED_ShowString(1, 1, "Clear Failed");
                OLED_ShowHexNum(2, 1, status, 2);
                NVIC_EnableIRQ(EXTI4_IRQn);
                Delay_ms(2000);
            }
            break;
        }
        else if (key == 3) // K3取消
        {
            break;
        }
    }
}

int main(void)
{
    // 1. 初始化各类基础外设
    OLED_Init();
    LED_Init();
    Key_Init();
    Servo_Init(); // ================= 新增：初始化舵机驱动控制 =================
    
    OLED_ShowString(1, 1, "System Initial...");
    
    // 初始化舵机至初始角度0° (代表闭锁/初始状态)
    Servo_SetAngle(0.0f);
    
    // 2. 初始化指纹模块通讯波特率与触摸检测中断
    ZW101_Init(57600); 
    ZW101_Touch_Init();  // 开启 PA4 WAKE 引脚触摸检测中断
    Delay_ms(1000);
    
    UI_ShowMenu();
    
    while (1)
    {
        // 捕获用户操作按键
        uint8_t key = Key_GetNum();
        
        if (key == 1) // PB0 (按键1) -> 切换运行选项
        {
            if (CurrentMode == MODE_VERIFY)
            {
                CurrentMode = MODE_ENROLL;
            }
            else if (CurrentMode == MODE_ENROLL)
            {
                CurrentMode = MODE_EMPTY;
            }
            else if (CurrentMode == MODE_EMPTY)
            {
                CurrentMode = MODE_VERIFY;
            }
            UI_ShowMenu();
        }
        else if (key == 2) // PB10 (按键2) -> 对应选项参数操作
        {
            if (CurrentMode == MODE_ENROLL)
            {
                EnrollID++;
                if (EnrollID > 99) EnrollID = 1;
                UI_ShowMenu();
            }
        }
        else if (key == 3) // PB11 (按键3) -> 确定确认进入该模式
        {
            if (CurrentMode == MODE_VERIFY)
            {
                Do_Verify();       // 进入验证检索
                Delay_ms(300);     // 退出后退避消抖300ms，严防因按键手抖快速“二次进入”导致系统卡死
                UI_ShowMenu();     // 退出后显示菜单
            }
            else if (CurrentMode == MODE_ENROLL)
            {
                Do_Enroll();       // 进入注册录入流程
                Delay_ms(300);     // 退避延时
                UI_ShowMenu();
            }
            else if (CurrentMode == MODE_EMPTY)
            {
                Do_EmptyLibrary(); // 格式化清空库
                Delay_ms(300);     // 退避延时
                UI_ShowMenu();
            }
        }
    }
}
