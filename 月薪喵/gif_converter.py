import os
from PIL import Image, ImageSequence, ImageFilter

# ==================== 配置参数 ====================
INPUT_GIF = "黄油小熊.GIF"          # 输入的动图文件名
OUTPUT_H_FILE = "User/gif_data1.h"  # 输出的 C 语言头文件名
TARGET_WIDTH = 64              # 缩放宽度
TARGET_HEIGHT = 64             # 缩放高度

THRESHOLD = 135                # 亮度分界阈值 (0-255)
# --------------------------------------------------
# 【新增参数】控制猫咪边线粗细：
# 1 = 原始粗细
# 3 = 中等加粗 (推荐，你的视频里就是这种效果)
# 5 = 特别加粗
LINE_THICKNESS = 1             
# --------------------------------------------------

INVERT_COLOR = True            # 是否反色（True：黑色背景，白色猫咪边线发光）
# ==================================================

def main():
    if not os.path.exists(INPUT_GIF):
        print(f"Error: 无法在当前文件夹下找到 '{INPUT_GIF}' 文件！")
        return

    img = Image.open(INPUT_GIF)
    frames_data = []
    durations = []
    
    print(f"开始转换 GIF (当前线条粗细设置为: {LINE_THICKNESS})...")
    
    # 逐帧处理 GIF
    for idx, frame in enumerate(ImageSequence.Iterator(img)):
        # 1. 【核心修复】解决透明通道(Transparency)导致反色后背景变白的问题：
        # 创建纯白背景，将每一帧通过 RGBA 混合贴在上面，确保背景和猫咪身体都是白色的，只有线条是深色的
        frame_rgba = frame.convert("RGBA")
        white_bg = Image.new("RGBA", frame_rgba.size, (255, 255, 255, 255))
        combined = Image.alpha_composite(white_bg, frame_rgba)
        
        # 2. 转换为灰度图像 (L 模式)
        frame_gray = combined.convert("L")
        
        # 3. 平滑缩放到目标尺寸
        frame_resized = frame_gray.resize((TARGET_WIDTH, TARGET_HEIGHT), Image.Resampling.LANCZOS)
        
        # 4. 将图像进行二值化（让猫咪的轮廓变成纯黑色[0]，身体和背景变成纯白色[255]）
        binary_frame = frame_resized.point(lambda p: 255 if p > THRESHOLD else 0)
        
        # 5. 使用最小滤波器（MinFilter）膨胀黑色像素，使边线变粗
        if LINE_THICKNESS > 1:
            size = LINE_THICKNESS if LINE_THICKNESS % 2 != 0 else LINE_THICKNESS + 1
            binary_frame = binary_frame.filter(ImageFilter.MinFilter(size))
            
        # 6. 颜色反转（黑色轮廓变成发光的白色，其余白色背景/身体变成纯黑色不发光）
        if INVERT_COLOR:
            binary_frame = binary_frame.point(lambda p: 255 - p)
            
        # 7. 打包为 SSD1306 页寻址格式
        pages = TARGET_HEIGHT // 8
        frame_bytes = []
        
        for p in range(pages):
            for x in range(TARGET_WIDTH):
                byte_val = 0
                for bit in range(8):
                    y = p * 8 + bit
                    pixel = binary_frame.getpixel((x, y))
                    
                    # 此时发光的白色(255)点亮 OLED 像素
                    if pixel > 127:
                        byte_val |= (1 << bit)
                frame_bytes.append(byte_val)
                
        frames_data.append(frame_bytes)
        duration = frame.info.get('duration', 100)
        durations.append(duration)
        print(f" -> 成功处理第 {idx+1:02d} 帧，延迟: {duration} ms")

    total_frames = len(frames_data)
    frame_size = (TARGET_WIDTH * TARGET_HEIGHT) // 8

    # 生成 C 语言头文件
    with open(OUTPUT_H_FILE, "w", encoding="utf-8") as f:
        f.write("/**\n")
        f.write(" * @file gif_data.h\n")
        f.write(f" * @brief 自动生成的 OLED GIF 动画数据, 共 {total_frames} 帧\n")
        f.write(f" * 尺寸: {TARGET_WIDTH}x{TARGET_HEIGHT} 像素, 线条粗细: {LINE_THICKNESS}\n")
        f.write(" */\n\n")
        f.write("#ifndef __GIF_DATA_H__\n")
        f.write("#define __GIF_DATA_H__\n\n")
        f.write("#include <stdint.h>\n\n")
        
        f.write(f"#define GIF_NUM_FRAMES   {total_frames}\n")
        f.write(f"#define GIF_WIDTH        {TARGET_WIDTH}\n")
        f.write(f"#define GIF_HEIGHT       {TARGET_HEIGHT}\n")
        f.write(f"#define GIF_FRAME_SIZE   {frame_size}\n\n")
        
        # 写入每一帧的数据
        for idx, frame in enumerate(frames_data):
            f.write(f"// 第 {idx+1} 帧图像数据\n")
            f.write(f"const uint8_t gif_frame_{idx}[{frame_size}] = {{\n    ")
            for i, b in enumerate(frame):
                f.write(f"0x{b:02X}, ")
                if (i + 1) % 16 == 0:
                    f.write("\n    ")
            f.write("\n};\n\n")
            
        # 写入帧指针索引数组
        f.write("// 帧指针数组\n")
        f.write("const uint8_t* const gif_frames[GIF_NUM_FRAMES] = {\n")
        for idx in range(total_frames):
            f.write(f"    gif_frame_{idx},\n")
        f.write("};\n\n")
        
        # 写入每帧延迟时间
        f.write("// 每帧延迟时间(ms)\n")
        f.write("const uint16_t gif_delays[GIF_NUM_FRAMES] = {\n    ")
        for d in durations:
            f.write(f"{d}, ")
        f.write("\n};\n\n")
        
        f.write("#endif // __GIF_DATA_H__\n")

    print(f"\n✨ 【成功生成 C 头文件：{OUTPUT_H_FILE}】")
    print(f" -> 线条加粗设置: {LINE_THICKNESS}")
    print(f" -> 总占用 Flash 空间: {(frame_size * total_frames) / 1024:.2f} KB")

if __name__ == "__main__":
    main()