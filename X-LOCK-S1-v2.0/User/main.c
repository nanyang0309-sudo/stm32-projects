#include "stm32f10x.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include "Delay.h"
#include "OLED.h"
#include "ZW101.h"
#include "Key.h"
#include "LED.h"
#include "Servo.h"

/* ============================================================
   安全常量定义
   ============================================================ */
#define MAX_FAILED_ATTEMPTS   5       // 最大连续失败次数
#define LOCKOUT_DURATION_MS   60000   // 锁定时间 60 秒
#define PASSWORD_LENGTH       4       // 密码位数
#define DEFAULT_PWD_D1        1       // 默认密码第1位
#define DEFAULT_PWD_D2        2       // 默认密码第2位
#define DEFAULT_PWD_D3        3       // 默认密码第3位
#define DEFAULT_PWD_D4        4       // 默认密码第4位

/* BKP 寄存器分配 (掉电可保持，用于持久化存储密码) */
#define BKP_PWD_MAGIC   BKP_DR1   // 魔数标记(0xA5A5表示密码已初始化)
#define BKP_PWD_D1      BKP_DR2   // 密码第1位 (0~9)
#define BKP_PWD_D2      BKP_DR3   // 密码第2位 (0~9)
#define BKP_PWD_D3      BKP_DR4   // 密码第3位 (0~9)
#define BKP_PWD_D4      BKP_DR5   // 密码第4位 (0~9)
#define PWD_MAGIC_VAL   0xA5A5

/* ============================================================
   系统模式枚举
   ============================================================ */
typedef enum {
    MODE_MAIN_MENU,        // 主菜单
    MODE_FINGERPRINT,      // 指纹验证（原 MODE_VERIFY）
    MODE_PASSWORD_UNLOCK,  // 密码后备解锁
    MODE_ADMIN_LOGIN,      // 管理员登录（输入密码）
    MODE_ADMIN_MENU,       // 管理员菜单
    MODE_ADMIN_ENROLL,     // 管理员：录入指纹
    MODE_ADMIN_CLEAR,      // 管理员：清空指纹库
    MODE_ADMIN_CHGPWD,     // 管理员：修改密码
    MODE_LOCKED            // 系统锁定
} SystemMode;

/* ============================================================
   全局变量
   ============================================================ */
SystemMode CurrentMode = MODE_MAIN_MENU;    // 当前系统模式
uint8_t MainMenuSel = 1;                     // 主菜单选中项 (1-3)
uint8_t AdminMenuSel = 1;                    // 管理员菜单选中项 (1-4)
uint8_t FailedAttempts = 0;                  // 连续失败计数
uint32_t LockStartTick = 0;                  // 锁定起始时刻(ms)
uint16_t EnrollID = 1;                       // 录入目标ID
uint8_t AdminPassword[PASSWORD_LENGTH];      // 管理员密码缓存 (运行时)
uint8_t IsAdminAuthed = 0;                   // 管理员已认证标志

/* ============================================================
   BKP 密码读写函数
   ============================================================ */

/**
  * @brief  初始化 BKP 区域时钟与后备域访问权限
  */
void BKP_Init_Password(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    // 检查魔数：若无，说明首次上电，写入默认密码
    if (BKP_ReadBackupRegister(BKP_PWD_MAGIC) != PWD_MAGIC_VAL)
    {
        BKP_WriteBackupRegister(BKP_PWD_D1, DEFAULT_PWD_D1);
        BKP_WriteBackupRegister(BKP_PWD_D2, DEFAULT_PWD_D2);
        BKP_WriteBackupRegister(BKP_PWD_D3, DEFAULT_PWD_D3);
        BKP_WriteBackupRegister(BKP_PWD_D4, DEFAULT_PWD_D4);
        BKP_WriteBackupRegister(BKP_PWD_MAGIC, PWD_MAGIC_VAL);
    }

    // 从 BKP 加载到 RAM 缓存
    AdminPassword[0] = (uint8_t)BKP_ReadBackupRegister(BKP_PWD_D1);
    AdminPassword[1] = (uint8_t)BKP_ReadBackupRegister(BKP_PWD_D2);
    AdminPassword[2] = (uint8_t)BKP_ReadBackupRegister(BKP_PWD_D3);
    AdminPassword[3] = (uint8_t)BKP_ReadBackupRegister(BKP_PWD_D4);
}

/**
  * @brief  将 RAM 中的密码写回 BKP 持久化
  */
void BKP_Save_Password(void)
{
    BKP_WriteBackupRegister(BKP_PWD_D1, AdminPassword[0]);
    BKP_WriteBackupRegister(BKP_PWD_D2, AdminPassword[1]);
    BKP_WriteBackupRegister(BKP_PWD_D3, AdminPassword[2]);
    BKP_WriteBackupRegister(BKP_PWD_D4, AdminPassword[3]);
}

/* ============================================================
   锁定管理函数
   ============================================================ */

/**
  * @brief  检查是否处于锁定状态；若锁定已过期则自动解除
  * @retval 1: 已锁定, 0: 未锁定
  */
uint8_t IsLocked(void)
{
    if (FailedAttempts < MAX_FAILED_ATTEMPTS)
        return 0;
    return 1; // 锁定中，由 Do_LockedMode 负责倒计时解除
}

/**
  * @brief  记录一次失败尝试；超阈值则锁定
  */
void RecordFailedAttempt(void)
{
    FailedAttempts++;
    if (FailedAttempts >= MAX_FAILED_ATTEMPTS)
    {
        LockStartTick = 0; // 标记锁定开始
        LED_Incorrect_ON();
    }
}

/**
  * @brief  验证成功后重置失败计数
  */
void ResetFailedAttempts(void)
{
    FailedAttempts = 0;
    LockStartTick = 0;
    LED_Incorrect_OFF();
}

/* ============================================================
   UI 辅助函数
   ============================================================ */

/**
  * @brief  主菜单显示
  */
void UI_MainMenu(void)
{
    NVIC_DisableIRQ(EXTI4_IRQn);
    NVIC_DisableIRQ(USART2_IRQn);

    OLED_Clear();
    OLED_ShowString(1, 1, "--- Main Menu ---");

    if (MainMenuSel == 1)
    {
        OLED_ShowString(2, 1, ">1.Fingerprint");
        OLED_ShowString(3, 1, " 2.Password");
        OLED_ShowString(4, 1, " 3.Admin");
    }
    else if (MainMenuSel == 2)
    {
        OLED_ShowString(2, 1, " 1.Fingerprint");
        OLED_ShowString(3, 1, ">2.Password");
        OLED_ShowString(4, 1, " 3.Admin");
    }
    else // sel == 3
    {
        OLED_ShowString(2, 1, " 1.Fingerprint");
        OLED_ShowString(3, 1, " 2.Password");
        OLED_ShowString(4, 1, ">3.Admin");
    }

    NVIC_EnableIRQ(USART2_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);
}

/**
  * @brief  管理员菜单显示
  */
void UI_AdminMenu(void)
{
    NVIC_DisableIRQ(EXTI4_IRQn);
    NVIC_DisableIRQ(USART2_IRQn);

    OLED_Clear();
    OLED_ShowString(1, 1, "--- Admin Menu ---");

    uint8_t sel = AdminMenuSel;
    OLED_ShowString(2, 1, (sel == 1) ? ">1.Enroll Finger" : " 1.Enroll Finger");
    OLED_ShowString(3, 1, (sel == 2) ? ">2.Clear Library" : " 2.Clear Library");
    OLED_ShowString(4, 1, (sel == 3) ? ">3.Change PWD"    : " 3.Change PWD");

    NVIC_EnableIRQ(USART2_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);
}

/**
  * @brief  密码输入 UI，返回 1 表示完成输入，返回 0 表示取消
  * @param  inputBuf: 输出缓冲区，存放输入结果(4字节)
  * @param  title: 屏幕标题
  * @retval 1: 输入完成, 0: 用户取消
  */
uint8_t UI_InputPassword(uint8_t *inputBuf, const char *title)
{
    uint8_t digits[PASSWORD_LENGTH] = {0, 0, 0, 0};
    uint8_t pos = 0;  // 当前正在编辑的位 (0~3)

    NVIC_DisableIRQ(EXTI4_IRQn);
    NVIC_DisableIRQ(USART2_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, (char *)title);
    OLED_ShowString(2, 1, "Enter 4-digit:");
    OLED_ShowString(4, 1, "K1:+ K2:OK K3:Esc");
    NVIC_EnableIRQ(USART2_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);

    // 显示初始值
    while (1)
    {
        // 刷新密码显示行
        NVIC_DisableIRQ(EXTI4_IRQn);
        NVIC_DisableIRQ(USART2_IRQn);
        OLED_ShowString(3, 1, "                "); // 清行
        for (uint8_t i = 0; i < PASSWORD_LENGTH; i++)
        {
            OLED_ShowNum(3, 1 + i * 4, digits[i], 1);
        }
        // 光标指示当前位置
        OLED_ShowString(3, 1 + pos * 4, "^");
        NVIC_EnableIRQ(USART2_IRQn);
        NVIC_EnableIRQ(EXTI4_IRQn);

        uint8_t key = Key_GetNum();
        if (key == 1)  // K1: 数字递增
        {
            digits[pos]++;
            if (digits[pos] > 9) digits[pos] = 0;
        }
        else if (key == 2)  // K2: 确认当前位
        {
            pos++;
            if (pos >= PASSWORD_LENGTH)
            {
                // 4位全部输完，确认
                for (uint8_t i = 0; i < PASSWORD_LENGTH; i++)
                    inputBuf[i] = digits[i];
                return 1;
            }
        }
        else if (key == 3)  // K3: 取消
        {
            return 0;
        }
        Delay_ms(10);
    }
}

/* ============================================================
   模式功能函数
   ============================================================ */

/**
  * @brief  指纹验证模式
  */
void Do_FingerprintVerify(void)
{
    uint8_t status;
    uint16_t matchedID = 0;
    uint16_t score = 0;

    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "[FINGERPRINT]");
    OLED_ShowString(2, 1, "Place Finger...");
    OLED_ShowString(4, 1, "K3:Back");
    NVIC_EnableIRQ(EXTI4_IRQn);

    while (1)
    {
        status = ZW101_GetImage();
        if (status == 0x00)
        {
            NVIC_DisableIRQ(EXTI4_IRQn);
            OLED_Clear();
            OLED_ShowString(1, 1, "Processing...");
            NVIC_EnableIRQ(EXTI4_IRQn);

            status = ZW101_GenChar(1);
            if (status == 0x00)
            {
                status = ZW101_Search(&matchedID, &score);
                if (status == 0x00)
                {
                    // 指纹匹配 → 进入密码二次验证
                    NVIC_DisableIRQ(EXTI4_IRQn);
                    OLED_Clear();
                    OLED_ShowString(1, 1, "Fingerprint OK!");
                    OLED_ShowString(2, 1, "ID: ");
                    OLED_ShowNum(2, 5, matchedID, 2);
                    OLED_ShowString(3, 1, "Enter PWD to open");
                    NVIC_EnableIRQ(EXTI4_IRQn);

                    // 指纹匹配成功，直接开门
                    ResetFailedAttempts();
                    NVIC_DisableIRQ(EXTI4_IRQn);
                    OLED_Clear();
                    OLED_ShowString(1, 1, "Access Granted!");
                    OLED_ShowString(2, 1, "ID: ");
                    OLED_ShowNum(2, 5, matchedID, 2);
                    OLED_ShowString(3, 1, "Score: ");
                    OLED_ShowNum(3, 8, score, 3);
                    OLED_ShowString(4, 1, "[Unlocking...]");
                    NVIC_EnableIRQ(EXTI4_IRQn);

                    LED_Correct_ON();
                    LED_Incorrect_OFF();
                    Servo_SetAngle(80.0f);
                    Delay_ms(3000);
                    Servo_SetAngle(0.0f);
                    LED_Correct_OFF();
                    break;
                }
                else
                {
                    // 验证失败
                    RecordFailedAttempt();
                    NVIC_DisableIRQ(EXTI4_IRQn);
                    OLED_Clear();
                    OLED_ShowString(1, 1, "Access Denied!");
                    OLED_ShowString(2, 1, "Not Recognized");
                    OLED_ShowString(3, 1, "Fails: ");
                    OLED_ShowNum(3, 8, FailedAttempts, 1);
                    OLED_ShowString(4, 1, "/5  Retry...");
                    NVIC_EnableIRQ(EXTI4_IRQn);

                    LED_Incorrect_ON();
                    Delay_ms(1500);
                    LED_Incorrect_OFF();
                }
            }
            else
            {
                NVIC_DisableIRQ(EXTI4_IRQn);
                OLED_Clear();
                OLED_ShowString(1, 1, "Bad Image");
                OLED_ShowString(2, 1, "Press harder");
                NVIC_EnableIRQ(EXTI4_IRQn);
                Delay_ms(1000);
            }

            // 检查是否触发锁定
            if (FailedAttempts >= MAX_FAILED_ATTEMPTS)
                break;

            // 刷新等待界面
            NVIC_DisableIRQ(EXTI4_IRQn);
            OLED_Clear();
            OLED_ShowString(1, 1, "[FINGERPRINT]");
            OLED_ShowString(2, 1, "Place Finger...");
            OLED_ShowString(4, 1, "K3:Back");
            NVIC_EnableIRQ(EXTI4_IRQn);
        }

        // K3 返回
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0)
        {
            Delay_ms(20);
            while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0);
            Delay_ms(20);
            break;
        }
    }
}

/**
  * @brief  密码后备解锁模式
  */
void Do_PasswordUnlock(void)
{
    uint8_t input[PASSWORD_LENGTH];

    // 检查是否已达最大失败次数
    if (FailedAttempts >= MAX_FAILED_ATTEMPTS)
        return;

    if (!UI_InputPassword(input, "Password Unlock"))
        return; // 用户取消

    // 比对密码
    uint8_t match = 1;
    for (uint8_t i = 0; i < PASSWORD_LENGTH; i++)
    {
        if (input[i] != AdminPassword[i])
        {
            match = 0;
            break;
        }
    }

    if (match)
    {
        ResetFailedAttempts();
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "Access Granted!");
        OLED_ShowString(2, 1, "Password OK");
        OLED_ShowString(4, 1, "[Unlocking...]");
        NVIC_EnableIRQ(EXTI4_IRQn);

        LED_Correct_ON();
        LED_Incorrect_OFF();
        Servo_SetAngle(80.0f);
        Delay_ms(3000);
        Servo_SetAngle(0.0f);
        LED_Correct_OFF();
    }
    else
    {
        RecordFailedAttempt();
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "Wrong Password!");
        OLED_ShowString(2, 1, "Fails: ");
        OLED_ShowNum(2, 8, FailedAttempts, 1);
        OLED_ShowString(3, 1, "/5  Try again...");
        NVIC_EnableIRQ(EXTI4_IRQn);

        LED_Incorrect_ON();
        Delay_ms(2000);
        LED_Incorrect_OFF();
    }
}

/**
  * @brief  管理员登录
  */
void Do_AdminLogin(void)
{
    uint8_t input[PASSWORD_LENGTH];

    if (FailedAttempts >= MAX_FAILED_ATTEMPTS)
        return;

    if (!UI_InputPassword(input, "Admin Login"))
        return;

    uint8_t match = 1;
    for (uint8_t i = 0; i < PASSWORD_LENGTH; i++)
    {
        if (input[i] != AdminPassword[i])
        {
            match = 0;
            break;
        }
    }

    if (match)
    {
        IsAdminAuthed = 1;
        ResetFailedAttempts();
        CurrentMode = MODE_ADMIN_MENU;
        AdminMenuSel = 1;

        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "Login Success!");
        OLED_ShowString(2, 1, "Welcome Admin");
        NVIC_EnableIRQ(EXTI4_IRQn);
        Delay_ms(1000);
    }
    else
    {
        RecordFailedAttempt();
        IsAdminAuthed = 0;
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "Wrong Password!");
        OLED_ShowString(2, 1, "Access Denied");
        OLED_ShowString(3, 1, "Fails: ");
        OLED_ShowNum(3, 8, FailedAttempts, 1);
        NVIC_EnableIRQ(EXTI4_IRQn);
        LED_Incorrect_ON();
        Delay_ms(2000);
        LED_Incorrect_OFF();
        CurrentMode = MODE_MAIN_MENU;
    }
}

/**
  * @brief  管理员：录入指纹
  */
void Do_AdminEnroll(void)
{
    uint8_t status;
    uint8_t cancelFlag = 0;

    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "--- Enroll ID ---");
    OLED_ShowString(2, 1, "Target ID: ");
    OLED_ShowNum(2, 12, EnrollID, 2);
    OLED_ShowString(3, 1, "Place Finger... ");
    OLED_ShowString(4, 1, "K3 to Cancel");
    NVIC_EnableIRQ(EXTI4_IRQn);

    // 第一次按压
    while (1)
    {
        status = ZW101_GetImage();
        if (status == 0x00) break;
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
        NVIC_EnableIRQ(EXTI4_IRQn);
        Delay_ms(1500);
        return;
    }

    status = ZW101_GenChar(1);
    if (status != 0x00)
    {
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "GenChar 1 Fail");
        OLED_ShowHexNum(2, 1, status, 2);
        NVIC_EnableIRQ(EXTI4_IRQn);
        Delay_ms(2000);
        return;
    }

    // 提示抬起
    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "Lift Finger...");
    LED_Correct_ON();
    Delay_ms(500);
    LED_Correct_OFF();
    NVIC_EnableIRQ(EXTI4_IRQn);

    while (ZW101_GetImage() == 0x00) { Delay_ms(100); }

    // 第二次按压
    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "Place Finger 2nd");
    OLED_ShowString(2, 1, "Same finger");
    NVIC_EnableIRQ(EXTI4_IRQn);

    cancelFlag = 0;
    while (1)
    {
        status = ZW101_GetImage();
        if (status == 0x00) break;
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
        NVIC_EnableIRQ(EXTI4_IRQn);
        Delay_ms(1500);
        return;
    }

    status = ZW101_GenChar(2);
    if (status != 0x00)
    {
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "GenChar 2 Fail");
        OLED_ShowHexNum(2, 1, status, 2);
        NVIC_EnableIRQ(EXTI4_IRQn);
        Delay_ms(2000);
        return;
    }

    // 合并保存
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
            OLED_ShowString(2, 1, "ID: ");
            OLED_ShowNum(2, 5, EnrollID, 2);
            NVIC_EnableIRQ(EXTI4_IRQn);

            for (int i = 0; i < 3; i++)
            {
                LED_Correct_ON();
                Delay_ms(200);
                LED_Correct_OFF();
                Delay_ms(100);
            }
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
        OLED_ShowString(2, 1, "Same finger?");
        NVIC_EnableIRQ(EXTI4_IRQn);
        LED_Incorrect_ON();
        Delay_ms(2000);
        LED_Incorrect_OFF();
    }
}

/**
  * @brief  管理员：清空指纹库（带二次确认）
  */
void Do_AdminClear(void)
{
    uint8_t key;

    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "!! WARNING !!");
    OLED_ShowString(2, 1, "Clear ALL prints?");
    OLED_ShowString(3, 1, "K1:Yes  K3:No");
    NVIC_EnableIRQ(EXTI4_IRQn);

    while (1)
    {
        key = Key_GetNum();
        if (key == 1)
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
                OLED_ShowString(2, 1, "Library Empty");
                NVIC_EnableIRQ(EXTI4_IRQn);
                for (int i = 0; i < 4; i++)
                {
                    GPIO_ResetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1);
                    Delay_ms(150);
                    GPIO_SetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1);
                    Delay_ms(150);
                }
                EnrollID = 1;
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
        else if (key == 3)
        {
            break;
        }
    }
}

/**
  * @brief  管理员：修改密码
  */
void Do_ChangePassword(void)
{
    uint8_t oldPwd[PASSWORD_LENGTH];
    uint8_t newPwd[PASSWORD_LENGTH];

    // 第一步：验证旧密码
    if (!UI_InputPassword(oldPwd, "Enter Old PWD"))
        return;

    uint8_t match = 1;
    for (uint8_t i = 0; i < PASSWORD_LENGTH; i++)
    {
        if (oldPwd[i] != AdminPassword[i]) { match = 0; break; }
    }
    if (!match)
    {
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "Wrong Old PWD!");
        NVIC_EnableIRQ(EXTI4_IRQn);
        LED_Incorrect_ON();
        Delay_ms(2000);
        LED_Incorrect_OFF();
        return;
    }

    // 第二步：输入新密码
    if (!UI_InputPassword(newPwd, "Enter New PWD"))
        return;

    // 第三步：确认新密码
    uint8_t confirm[PASSWORD_LENGTH];
    if (!UI_InputPassword(confirm, "Confirm New PWD"))
        return;

    uint8_t same = 1;
    for (uint8_t i = 0; i < PASSWORD_LENGTH; i++)
    {
        if (newPwd[i] != confirm[i]) { same = 0; break; }
    }
    if (!same)
    {
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_Clear();
        OLED_ShowString(1, 1, "Mismatch!");
        OLED_ShowString(2, 1, "Not changed");
        NVIC_EnableIRQ(EXTI4_IRQn);
        Delay_ms(2000);
        return;
    }

    // 保存新密码
    for (uint8_t i = 0; i < PASSWORD_LENGTH; i++)
        AdminPassword[i] = newPwd[i];
    BKP_Save_Password();

    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "Password Changed!");
    OLED_ShowString(2, 1, "New PWD saved");
    NVIC_EnableIRQ(EXTI4_IRQn);
    for (int i = 0; i < 3; i++)
    {
        LED_Correct_ON();
        Delay_ms(200);
        LED_Correct_OFF();
        Delay_ms(100);
    }
}

/**
  * @brief  锁定模式：显示倒计时并等待解除
  */
void Do_LockedMode(void)
{
    uint16_t remaining;

    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "!!! LOCKED !!!");
    OLED_ShowString(2, 1, "Too many fails");
    NVIC_EnableIRQ(EXTI4_IRQn);

    LED_Incorrect_ON();

    // 60秒倒计时
    for (uint16_t sec = 60; sec > 0; sec--)
    {
        remaining = sec;
        NVIC_DisableIRQ(EXTI4_IRQn);
        OLED_ShowString(3, 1, "Wait: ");
        OLED_ShowNum(3, 7, remaining, 2);
        OLED_ShowString(3, 10, "s  ");
        NVIC_EnableIRQ(EXTI4_IRQn);
        Delay_ms(1000);
    }

    // 锁定解除
    ResetFailedAttempts();
    LED_Incorrect_OFF();

    NVIC_DisableIRQ(EXTI4_IRQn);
    OLED_Clear();
    OLED_ShowString(1, 1, "Lock Released");
    OLED_ShowString(2, 1, "System Ready");
    NVIC_EnableIRQ(EXTI4_IRQn);
    Delay_ms(1500);

    CurrentMode = MODE_MAIN_MENU;
    MainMenuSel = 1;
}

/* ============================================================
   主函数
   ============================================================ */
int main(void)
{
    // 1. 初始化外设
    OLED_Init();
    LED_Init();
    Key_Init();
    Servo_Init();

    // 2. 初始化密码系统（BKP）
    BKP_Init_Password();

    // 3. 初始化指纹模块
    OLED_ShowString(1, 1, "System Init...");
    Servo_SetAngle(0.0f);
    ZW101_Init(57600);
    ZW101_Touch_Init();
    Delay_ms(800);

    // 4. 进入主菜单
    CurrentMode = MODE_MAIN_MENU;
    MainMenuSel = 1;
    IsAdminAuthed = 0;
    UI_MainMenu();

    // 5. 主循环
    while (1)
    {
        // 持续检测锁定状态
        if (FailedAttempts >= MAX_FAILED_ATTEMPTS)
        {
            CurrentMode = MODE_LOCKED;
            Do_LockedMode();
            // 锁定解除后回到主菜单
            UI_MainMenu();
            continue;
        }

        // 根据当前模式处理
        switch (CurrentMode)
        {
            /* ---------- 主菜单 ---------- */
            case MODE_MAIN_MENU:
            {
                uint8_t key = Key_GetNum();
                if (key == 1) // K1: 切换选项
                {
                    MainMenuSel++;
                    if (MainMenuSel > 3) MainMenuSel = 1;
                    UI_MainMenu();
                }
                else if (key == 3) // K3: 确认进入
                {
                    if (MainMenuSel == 1)
                        CurrentMode = MODE_FINGERPRINT;
                    else if (MainMenuSel == 2)
                        CurrentMode = MODE_PASSWORD_UNLOCK;
                    else if (MainMenuSel == 3)
                        CurrentMode = MODE_ADMIN_LOGIN;
                }
                break;
            }

            /* ---------- 指纹验证 ---------- */
            case MODE_FINGERPRINT:
                Do_FingerprintVerify();
                Delay_ms(300);
                UI_MainMenu();
                CurrentMode = MODE_MAIN_MENU;
                break;

            /* ---------- 密码后备解锁 ---------- */
            case MODE_PASSWORD_UNLOCK:
                Do_PasswordUnlock();
                Delay_ms(300);
                UI_MainMenu();
                CurrentMode = MODE_MAIN_MENU;
                break;

            /* ---------- 管理员登录 ---------- */
            case MODE_ADMIN_LOGIN:
                Do_AdminLogin();
                if (CurrentMode == MODE_ADMIN_MENU)
                {
                    UI_AdminMenu();
                }
                else
                {
                    // 登录失败回到主菜单
                    UI_MainMenu();
                }
                break;

            /* ---------- 管理员菜单 ---------- */
            case MODE_ADMIN_MENU:
            {
                uint8_t key = Key_GetNum();
                if (key == 1) // K1: 切换选项 (1-3循环)
                {
                    AdminMenuSel++;
                    if (AdminMenuSel > 3) AdminMenuSel = 1;
                    UI_AdminMenu();
                }
                else if (key == 2) // K2: 退出管理员模式
                {
                    IsAdminAuthed = 0;
                    CurrentMode = MODE_MAIN_MENU;
                    MainMenuSel = 1;
                    UI_MainMenu();
                }
                else if (key == 3) // K3: 确认进入
                {
                    if (AdminMenuSel == 1)
                        CurrentMode = MODE_ADMIN_ENROLL;
                    else if (AdminMenuSel == 2)
                        CurrentMode = MODE_ADMIN_CLEAR;
                    else if (AdminMenuSel == 3)
                        CurrentMode = MODE_ADMIN_CHGPWD;
                }
                break;
            }

            /* ---------- 管理员：录入 ---------- */
            case MODE_ADMIN_ENROLL:
                Do_AdminEnroll();
                Delay_ms(300);
                UI_AdminMenu();
                CurrentMode = MODE_ADMIN_MENU;
                break;

            /* ---------- 管理员：清库 ---------- */
            case MODE_ADMIN_CLEAR:
                Do_AdminClear();
                Delay_ms(300);
                UI_AdminMenu();
                CurrentMode = MODE_ADMIN_MENU;
                break;

            /* ---------- 管理员：改密 ---------- */
            case MODE_ADMIN_CHGPWD:
                Do_ChangePassword();
                Delay_ms(300);
                UI_AdminMenu();
                CurrentMode = MODE_ADMIN_MENU;
                break;

            /* ---------- 锁定 ---------- */
            case MODE_LOCKED:
                // 已在外部检测，此处兜底
                Do_LockedMode();
                UI_MainMenu();
                CurrentMode = MODE_MAIN_MENU;
                break;

            default:
                CurrentMode = MODE_MAIN_MENU;
                UI_MainMenu();
                break;
        }
    }
}
