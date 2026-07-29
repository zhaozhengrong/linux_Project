###  通过V4L2框架读取USB摄像头

# V4L2 摄像头采集流程

## 总体流程

```text
1. 打开设备
        |
        ↓
open("/dev/video0")

        |
        ↓

2. 查询设备能力

VIDIOC_QUERYCAP

确认：
- 是否支持视频采集
- 是否支持 Streaming 模式


        |
        ↓

3. 枚举支持的像素格式

VIDIOC_ENUM_FMT

得到：

- MJPEG
- YUYV
- NV12
...


        |
        ↓

4. 查询某种格式支持的分辨率

VIDIOC_ENUM_FRAMESIZES

例如：

MJPEG:
    2560×1440
    1920×1080
    1280×720

YUYV:
    1920×1080
    1280×720


        |
        ↓

5. 设置摄像头格式

VIDIOC_S_FMT


告诉驱动：

我要：

width = 1280
height = 720
pixelformat = MJPEG


        |
        ↓

6. 获取驱动最终采用的格式

VIDIOC_G_FMT


确认：

width = 1280
height = 720

pixelformat = MJPG

bytesperline = 0

sizeimage = 1843200


        |
        ↓

7. 申请 Buffer

VIDIOC_REQBUFS


        |
        ↓

8. 查询 Buffer 信息

VIDIOC_QUERYBUF


        |
        ↓

9. 内存映射

mmap()

将内核 Buffer 映射到用户空间


        |
        ↓

10. Buffer 放入驱动队列

VIDIOC_QBUF


        |
        ↓

11. 开始采集

VIDIOC_STREAMON


        |
        ↓

12. 等待数据

poll()


        |
        ↓

13. 获取一帧数据

VIDIOC_DQBUF


        |
        ↓

处理图像数据


        |
        ↓

14. 归还 Buffer

VIDIOC_QBUF
