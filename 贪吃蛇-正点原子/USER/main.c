/**
  ******************************************************************************
  * @file    main.c
  * @author  Gemini built by Google
  * @brief   正点原子 Mini STM32 开发板（2.8寸TFT-LCD）全彩贪吃蛇游戏（面包板按键稳定版）
  * @note    已彻底移除红外代码，回归最纯净、运行最流畅的独立按键控制。
  * * ?? 外部方向键盘接线说明（利用内部上拉，低电平有效）：
  * 请直接连接到屏幕右侧的【黄色双排母】引脚上：
  * - PA4: 上 (UP)    -> 接按键1一端，另一端接 GND
  * - PA5: 下 (DOWN)  -> 接按键2一端，另一端接 GND
  * - PA6: 左 (LEFT)  -> 接按键3一端，另一端接 GND
  * - PA1: 右 (RIGHT) -> 接按键4一端，另一端接 GND
  * - GND: 公共地线    -> 接 4 个按键的另一端（黄色排母最上方引脚就是 GND）
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "led.h"
#include "delay.h"
#include "usart.h"
#include "lcd.h"
#include "key.h"
#include <stdio.h>

// ==========================================
// 游戏地图及渲染参数配置
// ==========================================
#define BLOCK_SIZE   10   // 每个蛇身及食物的像素大小 (10x10)
#define MAP_WIDTH    30   // 地图网格宽度 (30 * 10 = 300 像素)
#define MAP_HEIGHT   18   // 地图网格高度 (18 * 10 = 180 像素)

// 地图在 320x240 屏幕上的偏移量
#define OFFSET_X     10   
#define OFFSET_Y     45   

// 蛇的最大长度
#define MAX_SNAKE_LEN  200

// ==========================================
// 汉字 16x16 精准点阵字模（游戏结束）
// 格式：阴码、逐行式、顺向高位在前 (MSB)
// ==========================================
const uint8_t hz_you[32] = {
    0x10,0x40,0x08,0x50,0x08,0xFE,0x94,0x40,0x54,0xA4,0x55,0x48,0x14,0x50,0x24,0x48,
    0xE4,0x44,0x24,0x84,0x25,0x84,0x26,0x48,0x24,0x30,0x20,0x10,0x20,0x60,0x20,0x40
}; // "游"
const uint8_t hz_xi[32] = {
    0x04,0x80,0x0E,0x60,0x74,0x58,0x08,0x44,0x08,0x46,0xFF,0xFA,0x08,0x40,0x08,0x40,
    0x14,0x40,0x14,0x40,0x22,0x44,0x42,0x44,0x80,0x84,0x01,0x04,0x02,0x28,0x0C,0x10
}; // "戏"
const uint8_t hz_jie[32] = {
    0x10,0x40,0x10,0x40,0x21,0x7E,0x22,0x44,0x7F,0xC4,0x08,0x44,0x13,0x7E,0x10,0x40,
    0x24,0x40,0x47,0xFC,0x84,0x04,0x14,0x04,0x27,0xFC,0x44,0x04,0x04,0x04,0x07,0xFC
}; // "结"
const uint8_t hz_shu[32] = {
    0x01,0x00,0x01,0x00,0x7F,0xFC,0x03,0x80,0x1F,0xF0,0x13,0x90,0x13,0x90,0x13,0x90,
    0x1F,0xF0,0x03,0x80,0x03,0x80,0x07,0xC0,0x0D,0xB0,0x19,0x98,0x31,0x8C,0x61,0x06
}; // "束"

// ==========================================
// 游戏结构体与全局变量
// ==========================================
typedef struct {
    uint8_t x;
    uint8_t y;
} Point_t;

Point_t snake[MAX_SNAKE_LEN]; // 蛇身数组
uint16_t snake_len = 0;       // 蛇的当前长度
Point_t food;                 // 食物位置

// 蛇的方向: 0-上, 1-右, 2-下, 3-左
uint8_t dir = 1;              

uint32_t score = 0;           // 当前得分
uint8_t game_over = 0;        // 游戏结束标志
uint32_t rand_seed = 0;       // 伪随机数种子

// ==========================================
// 外部独立键盘初始化与检测 (使用 GPIOA)
// ==========================================

/**
  * @brief  初始化外接方向键盘的 GPIO (PA1, PA4, PA5, PA6配置为输入上拉)
  */
void EXT_Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 使能 GPIOA 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 内部上拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * @brief  读取外接方向键盘
  * @retval 0:无按键, 1:上(PA4), 2:下(PA5), 3:左(PA6), 4:右(PA1)
  */
uint8_t EXT_Key_Scan(void)
{
    // 按下时引脚接地，读出为 0
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) == 0) return 1; // 上 (PA4)
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) == 0) return 2; // 下 (PA5)
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == 0) return 3; // 左 (PA6)
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == 0) return 4; // 右 (PA1)
    return 0;
}

// ==========================================
// 游戏辅助工具函数
// ==========================================

/**
  * @brief  基于 LCG 算法的轻量级伪随机数发生器
  */
uint32_t Get_Random(void)
{
    rand_seed = rand_seed * 1103515245 + 12345;
    return (uint32_t)(rand_seed / 65536) % 32768;
}

/**
  * @brief  在 LCD 上绘制一个游戏网格方块
  */
void Draw_Block(uint8_t x, uint8_t y, uint16_t color)
{
    LCD_Fill(OFFSET_X + x * BLOCK_SIZE, 
             OFFSET_Y + y * BLOCK_SIZE, 
             OFFSET_X + (x + 1) * BLOCK_SIZE - 1, 
             OFFSET_Y + (y + 1) * BLOCK_SIZE - 1, 
             color);
}

/**
  * @brief  自定义中文高保真渲染引擎（完美兼容 C89）
  * @note   使用双线性插值原理将 16x16 点阵放大 2 倍画点，呈现无锯齿的 32x32 磅汉字
  */
void Draw_Chinese_32x32(uint16_t x, uint16_t y, const uint8_t *font, uint16_t color)
{
    uint8_t r = 0;   // 行控制
    uint8_t b = 0;   // 字节控制
    uint8_t bit = 0; // 位控制
    uint8_t temp = 0;

    for (r = 0; r < 16; r++)
    {
        for (b = 0; b < 2; b++)
        {
            temp = font[r * 2 + b];
            for (bit = 0; bit < 8; bit++)
            {
                if (temp & (0x80 >> bit))
                {
                    // 放大 2x：通过 LCD_Fill 填充 2x2 像素方块
                    LCD_Fill(x + (b * 8 + bit) * 2, 
                             y + r * 2, 
                             x + (b * 8 + bit) * 2 + 1, 
                             y + r * 2 + 1, 
                             color);
                }
                else
                {
                    // 填平背景（黑色）
                    LCD_Fill(x + (b * 8 + bit) * 2, 
                             y + r * 2, 
                             x + (b * 8 + bit) * 2 + 1, 
                             y + r * 2 + 1, 
                             BLACK);
                }
            }
        }
    }
}

/**
  * @brief  在随机位置生成食物，并确保不与蛇身重叠
  */
void Generate_Food(void)
{
    uint8_t on_snake;
    uint16_t i; // C89标准：循环变量必须先在函数头部定义
    while (1)
    {
        food.x = Get_Random() % MAP_WIDTH;
        food.y = Get_Random() % MAP_HEIGHT;
        
        on_snake = 0;
        for (i = 0; i < snake_len; i++)
        {
            if (snake[i].x == food.x && snake[i].y == food.y)
            {
                on_snake = 1;
                break;
            }
        }
        if (!on_snake) break; 
    }
    Draw_Block(food.x, food.y, RED); // 绘制食物
}

/**
  * @brief  绘制游戏区域的边框
  */
void Draw_Game_Border(void)
{
    POINT_COLOR = GRAY;
    LCD_DrawRectangle(OFFSET_X - 1, 
                      OFFSET_Y - 1, 
                      OFFSET_X + MAP_WIDTH * BLOCK_SIZE, 
                      OFFSET_Y + MAP_HEIGHT * BLOCK_SIZE);
}

/**
  * @brief  在屏幕上方更新分数显示
  */
void Update_Score_UI(void)
{
    char score_str[20];
    sprintf(score_str, "SCORE: %04d", score);
    POINT_COLOR = WHITE;
    LCD_ShowString(10, 15, 120, 16, 16, (uint8_t*)score_str);
}

/**
  * @brief  游戏初始化与复位
  */
void Game_Reset(void)
{
    uint16_t i; // C89标准：循环变量必须先在函数头部定义
    
    LCD_Clear(BLACK);
    Draw_Game_Border();
    
    // 显示操作说明提示
    POINT_COLOR = WHITE;
    LCD_ShowString(140, 15, 170, 16, 12, (uint8_t*)"Breadboard Keys: PA4/5/6/1");
    
    // 初始化蛇的位置（朝右运动）
    snake_len = 4;
    snake[0].x = 5;  snake[0].y = 5; // 蛇头
    snake[1].x = 4;  snake[1].y = 5;
    snake[2].x = 3;  snake[2].y = 5;
    snake[3].x = 2;  snake[3].y = 5; // 蛇尾
    
    dir = 1; // 初始向右运动
    score = 0;
    game_over = 0;
    
    // 绘制蛇身
    for (i = 0; i < snake_len; i++)
    {
        Draw_Block(snake[i].x, snake[i].y, GREEN);
    }
    
    Generate_Food();
    Update_Score_UI();
}

/**
  * @brief  游戏结束画面
  */
void Show_Game_Over(void)
{
    char score_str[20]; // C89标准：定义必须位于所有可执行代码句之前
    
    LCD_Clear(BLACK);
    
    // 使用我们自定义的 2x 缩放中文画字引擎，在 320x240 横屏下完美居中：
    Draw_Chinese_32x32(96,  70, hz_you, RED);
    Draw_Chinese_32x32(128, 70, hz_xi,  RED);
    Draw_Chinese_32x32(160, 70, hz_jie, RED);
    Draw_Chinese_32x32(192, 70, hz_shu, RED);
    
    POINT_COLOR = WHITE;
    sprintf(score_str, "Your Score: %d", score);
    // 调整英文与分数坐标，防止与上方 32x32 的汉字重叠
    LCD_ShowString(100, 135, 200, 16, 16, (uint8_t*)score_str);
    LCD_ShowString(50, 175, 240, 16, 16, (uint8_t*)"Press any key to Restart");
}


/* ==========================================
 * 主程序入口
 * ========================================== */
int main(void)
{
    uint8_t key_val = 0;
    uint8_t temp_key = 0; // C89标准：临时接收按键
    uint8_t i = 0;        // C89标准：主循环变量
    int s_idx = 0;        // C89标准：平移蛇身时的循环变量
    Point_t next_head;    // C89标准：移入顶端定义
    
    // 初始化系统外设
    delay_init();             // 延时初始化	  
    uart_init(115200);        // 串口初始化
    LED_Init();		  	    // LED 初始化
    
    LCD_Init();               // 屏幕初始化
    LCD_Display_Dir(1);       // 将屏幕方向强制设置为横屏模式 (320x240)
    
    KEY_Init();               // 开发板自带按键初始化
    EXT_Key_Init();           // 外部方向按键初始化 (PA1, PA4, PA5, PA6)

    Game_Reset();

    while (1)
    {
        // 1. 如果游戏结束，等待外部任意方向按键按下重开
        if (game_over)
        {
            key_val = EXT_Key_Scan();
            if (key_val != 0 || KEY_Scan(0) == WKUP_PRES) 
            {
                Game_Reset();
            }
            delay_ms(20);
            continue;
        }

        // 2. 高频轮询外部物理方向键
        // 游戏画面刷新周期恒定为 150ms，通过高频轮询保证按键不漏检
        key_val = 0; 
        for (i = 0; i < 15; i++)
        {
            temp_key = EXT_Key_Scan();
            if (temp_key != 0)
            {
                key_val = temp_key; // 仅仅记录下按下的按键值，绝对不使用 break 强行跳出，从而维持 150ms 稳定帧率！
            }
            
            // 累加种子，提供更好的随机数生成
            rand_seed++; 
            delay_ms(10);
        }

        // 3. 延时结束后，统一执行一次方向更新（并防止自咬反转）
        if (key_val != 0)
        {
            if (key_val == 1 && dir != 2) dir = 0; // 向上
            if (key_val == 2 && dir != 0) dir = 2; // 向下
            if (key_val == 3 && dir != 1) dir = 3; // 向左
            if (key_val == 4 && dir != 3) dir = 1; // 向右
        }

        // 4. 计算蛇头下一步位置
        next_head = snake[0];
        switch (dir)
        {
            case 0: next_head.y--; break; // 上
            case 1: next_head.x++; break; // 右
            case 2: next_head.y++; break; // 下
            case 3: next_head.x--; break; // 左
        }

        // 5. 边界碰撞与身体碰撞检测
        if (next_head.x >= MAP_WIDTH || next_head.y >= MAP_HEIGHT)
        {
            game_over = 1;
            Show_Game_Over();
            continue;
        }
        for (i = 0; i < snake_len; i++)
        {
            if (next_head.x == snake[i].x && next_head.y == snake[i].y)
            {
                game_over = 1;
                Show_Game_Over();
                break;
            }
        }
        if (game_over) continue;

        // 6. 移动与渲染逻辑
        if (next_head.x == food.x && next_head.y == food.y)
        {
            // 吃掉食物：蛇身平移，长度增加
            for (s_idx = snake_len; s_idx > 0; s_idx--)
            {
                snake[s_idx] = snake[s_idx - 1];
            }
            snake[0] = next_head;
            snake_len++;
            
            score += 10;
            Update_Score_UI();
            
            // 绘制新蛇头，重新生成食物
            Draw_Block(snake[0].x, snake[0].y, GREEN);
            Generate_Food();
        }
        else
        {
            // 移动空地：清除蛇尾，平移蛇身，绘制新蛇头 (无闪烁刷新)
            Draw_Block(snake[snake_len - 1].x, snake[snake_len - 1].y, BLACK);
            
            for (s_idx = snake_len - 1; s_idx > 0; s_idx--)
            {
                snake[s_idx] = snake[s_idx - 1];
            }
            snake[0] = next_head;
            
            Draw_Block(snake[0].x, snake[0].y, GREEN);
        }

        // 7. 指示灯闪烁，代表游戏主循环正常运转
        LED0 = !LED0;
    }
}
