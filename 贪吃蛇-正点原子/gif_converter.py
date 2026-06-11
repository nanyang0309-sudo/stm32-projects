import os
from PIL import Image, ImageSequence

# ==================== 配置参数 ====================
INPUT_GIF = "cat.GIF"               # 输入的动图文件名
OUTPUT_H_FILE = "gif_color_data.h"  # 输出的彩色 C 语言头文件名

TARGET_WIDTH = 64                  # 压缩到 64x64 (单帧只需 8KB)
TARGET_HEIGHT = 64                 

# 【核心优化】最大允许保留的帧数 (12帧足够丝滑流畅，同时体积极小)
MAX_ALLOWED_FRAMES = 12            
# ==================================================

def convert_to_rgb565(pixel):
    """将 (R, G, B) 像素转换为正点原子 LCD 要求的 16位 RGB565 格式"""
    r, g, b = pixel[:3]
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def main():
    if not os.path.exists(INPUT_GIF):
        print(f"Error: 无法找到 '{INPUT_GIF}' 文件！")
        return

    img = Image.open(INPUT_GIF)
    
    # 获取原始 GIF 的所有帧
    raw_frames = [f.copy() for f in ImageSequence.Iterator(img)]
    total_raw_frames = len(raw_frames)
    
    # 计算抽稀步长，确保总帧数不超过 MAX_ALLOWED_FRAMES
    step = 1
    if total_raw_frames > MAX_ALLOWED_FRAMES:
        step = total_raw_frames / MAX_ALLOWED_FRAMES
    
    frames_data = []
    durations = []
    
    print(f"原始动画共 {total_raw_frames} 帧，正在自动抽稀并转换为 RGB565...")
    
    for i in range(MAX_ALLOWED_FRAMES):
        # 线性插值计算抽取哪一帧
        target_idx = int(i * step)
        if target_idx >= total_raw_frames:
            break
            
        frame = raw_frames[target_idx]
        frame_rgb = frame.convert("RGB")
        frame_resized = frame_rgb.resize((TARGET_WIDTH, TARGET_HEIGHT), Image.Resampling.LANCZOS)
        
        frame_pixels = []
        for y in range(TARGET_HEIGHT):
            for x in range(TARGET_WIDTH):
                pixel = frame_resized.getpixel((x, y))
                frame_pixels.append(convert_to_rgb565(pixel))
                
        frames_data.append(frame_pixels)
        # 累加被跳过帧的延迟时间，确保动画播放速度正常
        duration = frame.info.get('duration', 100)
        durations.append(int(duration * step))
        print(f" -> 成功提取并压缩第 {i+1:02d} 帧 (对应原图第 {target_idx+1:02d} 帧)")

    total_frames = len(frames_data)
    pixels_per_frame = TARGET_WIDTH * TARGET_HEIGHT

    # 生成 C 语言头文件
    with open(OUTPUT_H_FILE, "w", encoding="utf-8") as f:
        f.write("/**\n")
        f.write(" * @file gif_color_data.h\n")
        f.write(f" * @brief 自动生成的正点原子 TFT-LCD 彩色 GIF 瘦身数据, 共 {total_frames} 帧\n")
        f.write(" */\n\n")
        f.write("#ifndef __GIF_COLOR_DATA_H__\n")
        f.write("#define __GIF_COLOR_DATA_H__\n\n")
        f.write("#include <stdint.h>\n\n")
        
        f.write(f"#define GIF_NUM_FRAMES   {total_frames}\n")
        f.write(f"#define GIF_WIDTH        {TARGET_WIDTH}\n")
        f.write(f"#define GIF_HEIGHT       {TARGET_HEIGHT}\n\n")
        
        # 写入每一帧的数据
        for idx, frame in enumerate(frames_data):
            f.write(f"// 第 {idx+1} 帧彩色点阵数据\n")
            f.write(f"const uint16_t gif_frame_{idx}[{pixels_per_frame}] = {{\n    ")
            for i, rgb565 in enumerate(frame):
                f.write(f"0x{rgb565:04X}, ")
                if (i + 1) % 12 == 0:
                    f.write("\n    ")
            f.write("\n};\n\n")
            
        # 写入帧指针数组
        f.write("const uint16_t* const gif_frames[GIF_NUM_FRAMES] = {\n")
        for idx in range(total_frames):
            f.write(f"    gif_frame_{idx},\n")
        f.write("};\n\n")
        
        # 写入每帧延迟时间
        f.write("const uint16_t gif_delays[GIF_NUM_FRAMES] = {\n    ")
        for d in durations:
            f.write(f"{d}, ")
        f.write("\n};\n\n")
        
        f.write("#endif // __GIF_COLOR_DATA_H__\n")

    print(f"\n✨ 【成功生成超轻量彩色头文件：{OUTPUT_H_FILE}】")
    print(f" -> 优化后尺寸: {TARGET_WIDTH}x{TARGET_HEIGHT}")
    print(f" -> 优化后帧数: {total_frames} 帧")
    print(f" -> 【核心指标】预计占用 Flash 空间: {(pixels_per_frame * 2 * total_frames) / 1024:.2f} KB (大暴降！)")

if __name__ == "__main__":
    main()