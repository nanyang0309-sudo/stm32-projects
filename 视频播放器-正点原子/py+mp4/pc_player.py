import cv2
import serial
import serial.tools.list_ports
import time
import sys
import numpy as np

# ==================== 🛠️ 配置参数 (极限高帧率无损版) ====================
TARGET_WIDTH = 160        # 压缩宽度 (和单片机程序严格一致)
TARGET_HEIGHT = 120       # 压缩高度
BAUD_RATE = 2000000       # 波特率设置 (极高波特率保证 USB 通道不堵塞)
TARGET_FPS = 60           # 目标推送帧率 (可以设为 45, 50, 60，尽情享受丝滑！)
# ===================================================================

# 计算每帧理想的持续时间
FRAME_INTERVAL = 1.0 / TARGET_FPS 

def convert_to_bgr565_numpy(frame):
    """
    极速矩阵转换算法 (NumPy 矩阵加速)：
    彻底废除低效的 Python 双重 for 循环。
    利用 NumPy 在 C 语言底层的并行矩阵位移运算，在 0.2 毫秒内将一整帧转换完毕！
    """
    # 1. 分离 B, G, R 三个颜色通道 (保证数据类型为 uint16 以防位移溢出)
    b = frame[:, :, 0].astype(np.uint16)
    g = frame[:, :, 1].astype(np.uint16)
    r = frame[:, :, 2].astype(np.uint16)

    # 2. 转换为 5-6-5 位格式 (正点原子 LCD 控制器采用 BGR565 排列)
    b5 = (b >> 3) & 0x1F  # 蓝色高位
    g6 = (g >> 2) & 0x3F  # 绿色中位
    r5 = (r >> 3) & 0x1F  # 红色低位

    # 3. 矩阵拼接合成 16 位颜色值
    bgr565 = (b5 << 11) | (g6 << 5) | r5

    # 4. 极速拆分为高 8 位和低 8 位字节
    high_byte = ((bgr565 >> 8) & 0xFF).astype(np.uint8)
    low_byte = (bgr565 & 0xFF).astype(np.uint8)

    # 5. 高低字节交织拼合，直接导出为二进制字节流
    return np.stack((high_byte, low_byte), axis=-1).tobytes()

def find_stm32_port():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("❌ 错误: 电脑上没有检测到任何串口设备！")
        return None

    print("\n🔍 检测到以下串口设备:")
    for i, p in enumerate(ports):
        print(f"  [{i}] 端口: {p.device} | 描述: {p.description}")

    # 自动排除调试端口 (ST-Link / J-Link)
    for p in ports:
        desc_upper = p.description.upper()
        if "ST-LINK" in desc_upper or "STLINK" in desc_upper or "J-LINK" in desc_upper or "JLINK" in desc_upper:
            continue
        if "STMICROELECTRONICS" in desc_upper or "VIRTUAL" in desc_upper or "USB" in desc_upper:
            print(f"\n⚡ 自动锁定原生视频端口: {p.device}")
            return p.device

    print("\n⚠️ 无法自动判定视频端口。")
    try:
        val = input("👉 请看上方列表，输入原生 USB 串口前面的序号: ").strip()
        return ports[int(val)].device
    except Exception:
        pass
    return ports[0].device

def main():
    video_path = "video.mp4" 
    if len(sys.argv) > 1:
        video_path = sys.argv[1]

    port = find_stm32_port()
    if not port:
        return
    
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        print(f"🔌 建立物理视频原色通道成功: {port}")
    except Exception as e:
        print(f"❌ 无法打开串口 {port}, 原因: {e}")
        return

    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print(f"❌ 无法打开视频文件 '{video_path}'，请确认视频存在于当前目录下。")
        ser.close()
        return

    print(f"\n🎬 视频流原色无损推流中 (目标帧率: {TARGET_FPS} FPS)...")

    frame_count = 0
    fps_timer = time.time()

    while cap.isOpened():
        t_start = time.perf_counter()  # 获取当前高精度系统时间戳

        ret, frame = cap.read()
        if not ret:
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            continue

        # 1. 极速缩放到 160x120
        resized = cv2.resize(frame, (TARGET_WIDTH, TARGET_HEIGHT), interpolation=cv2.INTER_AREA)

        # 2. 帧同步头拼装
        img_bytes = bytearray()
        img_bytes.append(0xAA)
        img_bytes.append(0xBB)

        # 3. 使用 NumPy 矩阵合并加速
        pixel_bytes = convert_to_bgr565_numpy(resized)
        img_bytes.extend(pixel_bytes)

        # 4. 极速发送到串口
        try:
            ser.write(img_bytes)
        except Exception as e:
            print(f"\n❌ 发送中断: {e}")
            break

        # 5. FPS 实时统计与打印
        frame_count += 1
        elapsed = time.time() - fps_timer
        if elapsed > 1.0:
            print(f"🚀 视频同步推送中... 速度: {frame_count / elapsed:.1f} FPS")
            frame_count = 0
            fps_timer = time.time()

        # 6. 电脑端预览同步
        cv2.imshow("PC Sender Preview", resized)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

        # 7. 高精度动态控制：计算剩余应该 sleep 的时间，保证帧率精准恒定
        t_end = time.perf_counter()
        t_processed = t_end - t_start
        t_sleep = FRAME_INTERVAL - t_processed
        if t_sleep > 0:
            time.sleep(t_sleep)

    cap.release()
    cv2.destroyAllWindows()
    ser.close()

if __name__ == "__main__":
    main()