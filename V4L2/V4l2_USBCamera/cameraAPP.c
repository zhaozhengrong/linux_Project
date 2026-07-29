#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <poll.h>


#define REQBUFS_CNT     3

typedef struct 
{  
    int fd;
    struct v4l2_capability  capability;    /* 获取设备能力信息 */
    struct v4l2_fmtdesc fmtdesc;            /* 枚举（遍历）摄像头支持的像素格式 */
    struct v4l2_frmsizeenum fsenum;    /* 查询某一种像素格式支持哪些分辨率*/
    struct v4l2_format  fmt;                        /* 设置像素格式 */
    struct v4l2_requestbuffers rb;          /* 申请buffer*/
    struct v4l2_buffer  buf;                         /*  video buffer info */
    void *bufs[REQBUFS_CNT];
}camera;

camera  cam;


int camera_open_dev(camera *cam,char *dev)
{
    cam->fd = open(dev,O_RDWR);
    if(cam->fd < 0)
    {
        printf("Usage: %s </dev/videoX>,print format for video \n",dev);
        return -1;
    }
}

int camera_set_formats(camera *cam)
{
    memset(&cam->fmt,0,sizeof(cam->fmt));
    cam->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->fmt.fmt.pix.width = 1280;
    cam->fmt.fmt.pix.height = 720;
    cam->fmt.fmt.pix.pixelformat =V4L2_PIX_FMT_JPEG;
    cam->fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if(ioctl(cam->fd,VIDIOC_S_FMT,&cam->fmt) == 0)
    {
        printf("set format ok:%d x %d\n",cam->fmt.fmt.pix.width,cam->fmt.fmt.pix.height);
        printf("Set format successful! sizeimage = %d\n", cam->fmt.fmt.pix.sizeimage);
    }else
    {
        printf("can not set format\n");    
        return -1;    
    }
    
    return 0; 
}


int camera_get_formats(camera *cam)
{
    memset(&cam->fmt,0,sizeof(cam->fmt));

    cam->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(cam->fd,VIDIOC_G_FMT,&cam->fmt) < 0)
    {
        printf("can not get format\r\n");
    }

    printf("驱动最后给的格式 ===\n");
    printf("width           = %u\n",   cam->fmt.fmt.pix.width);
    printf("height          = %u\n",   cam->fmt.fmt.pix.height);
    printf("pixelformat     = %c%c%c%c\n",
        cam->fmt.fmt.pix.pixelformat & 0xff,
        (cam->fmt.fmt.pix.pixelformat >> 8) & 0xff,
        (cam->fmt.fmt.pix.pixelformat >> 16) & 0xff,
        (cam->fmt.fmt.pix.pixelformat >> 24) & 0xff);
    printf("bytesperline    = %u  这最重要！\n", cam->fmt.fmt.pix.bytesperline);
    printf("sizeimage       = %u\n",   cam->fmt.fmt.pix.sizeimage);
    printf("colorspace      = %d\n",   cam->fmt.fmt.pix.colorspace);
    printf("flags           = 0x%08x\n", cam->fmt.fmt.pix.flags);

    return 0; 
}



int camera_enum_formats(camera *cam,const char *dev)
{
    int fmt_index = 0;
    int frame_index = 0;

    /* 清空capability */
    memset(&cam->capability,0,sizeof(cam->capability));


    /* ioctl函数的第三个参数的类型取决于第二个参数的类型*/
    if(ioctl(cam->fd,VIDIOC_QUERYCAP,&cam->capability) == 0)
    {
        /* 1. 必须支持视频捕获*/
        if((cam->capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
        {
             printf("Error opening device %s:video capture not supported\n",dev);
            return -1;
        }

        /*2.判断当前设备支持Streaming*/

        /* V4L2 有几种 IO 方式：
            方式1：read/write模式
            方式2：Streaming模式
        */
        if((cam->capability.capabilities & V4L2_CAP_STREAMING) == 0)
        {
            printf("%s does not support streaming i/o \n",dev);
            return -1;
        }
    }else
    {
         printf("can not get capability\r\n");
    }

     /* 2.外层循环：枚举当前设备支持的像素格式 */
    while (1)
    {
        /* 设置要查询的格式索引 */
        cam->fmtdesc.index = fmt_index;
        /* 指定设备类型：视频采集（摄像头） */
        cam->fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        /*
        * 调用 VIDIOC_ENUM_FMT：
        * 向驱动查询：
        * 第 fmt_index 个
        * VIDEO_CAPTURE 类型
        * 支持的像素格式
        * 成功：驱动会填充 fmtdesc 结构体
        * 失败：说明没有更多格式，退出循环
        */
        if(ioctl(cam->fd,VIDIOC_ENUM_FMT,&cam->fmtdesc) != 0)break;

         frame_index = 0;

        /*3.内层循环 ：遍历当前格式所支持的分辨率*/
        while (1)
        {
            memset(&cam->fsenum,0,sizeof(cam->fsenum));
            cam->fsenum.pixel_format = cam->fmtdesc.pixelformat;
            cam->fsenum.index = frame_index;

             if(ioctl(cam->fd,VIDIOC_ENUM_FRAMESIZES,&cam->fsenum) == 0)
            {
                /*
                * fmtdesc.description  → 格式名称
                * fmtdesc.pixelformat  → 像素格式编码
                * frame_index          → 分辨率索引
                * fsenum.discrete.width  → 宽
                * fsenum.discrete.height → 高
                */
                printf("format = %s %d  framesize %d,%d x %d \r\n",cam->fmtdesc.description,cam->fmtdesc.pixelformat,
                     frame_index,cam->fsenum.discrete.width,cam->fsenum.discrete.height);
            }else
            {
                break;
            }
            /* 分辨率索引递增 */
            frame_index++;
        }
         /* 像素格式索引递增 */
        fmt_index++;   
    }
     
    return 0;
}

int camera_init_buffers(camera *cam)
{
     int buf_cnt = 0;
    int i = 0;

     /* 1.申请buffer  让驱动在内核空间分配 N 个 buffer*/
    memset(&cam->rb,0,sizeof(cam->rb));
    /* 指定申请的缓冲区数量 */
    cam->rb.count = REQBUFS_CNT;  

    /*
     * 指定缓冲区类型
     * V4L2_BUF_TYPE_VIDEO_CAPTURE
     * 表示视频采集缓冲区（摄像头采集）
     */
    cam->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 

    /*
     * 指定缓冲区内存类型
     * V4L2_MEMORY_MMAP
     * 表示使用内存映射方式（mmap）
     * buffer 由驱动在内核分配，然后映射到用户空间
    */
    cam->rb.memory = V4L2_MEMORY_MMAP;

      if(ioctl(cam->fd,VIDIOC_REQBUFS,&cam->rb) == 0)
    {
        /* 2.申请成功之后,内存在 驱动（内核）里 现在还碰不到 */
        printf("driver allocated %d buffers\n", cam->rb.count);
        buf_cnt = cam->rb.count;
        for(i = 0; i < cam->rb.count; i++)
        {
            /*3.既然申请成功了 这一步根据　下标　类型和内存类型来查询*/
            memset(&cam->buf,0,sizeof(cam->buf));
            cam->buf.index = i;
            cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            cam->buf.memory = V4L2_MEMORY_MMAP;
            if(ioctl(cam->fd,VIDIOC_QUERYBUF,&cam->buf) == 0)
            {
                /*4.查询成功之后　就可以内存映射了*/
                /*
                * 使用 mmap 将内核中的 buffer 映射到用户空间
                * 参数说明：
                * NULL               -> 让系统自动分配映射地址
                * cam->buf.length   -> 映射大小（必须用驱动给的）
                * PROT_READ|WRITE   -> 可读可写
                * MAP_SHARED        -> 用户空间和内核共享同一块内存
                * cam->fd           -> 设备文件描述符
                * cam->buf.m.offset -> 第 i 个 buffer 的偏移（关键！）
                */

               /*
                    cam->bufs[0]  ---> 第0个buffer在用户空间的地址
                    cam->bufs[1]  ---> 第1个buffer在用户空间的地址
                    cam->bufs[2]  ---> 第2个buffer在用户空间的地址
                    cam->bufs[3]  ---> 第3个buffer在用户空间的地址
               */
                cam->bufs[i] = mmap(0,cam->buf.length,PROT_READ|PROT_WRITE|O_TRUNC,MAP_SHARED,cam->fd,cam->buf.m.offset);
                if(cam->bufs[i] == MAP_FAILED)
                {
                    printf("Unable to map buffer\n");
                    return -1;
                }
            }else
            {
                printf("can not query buffer\n");
                return -1;
            }     
        }
        printf("map %d buffers ok\n",buf_cnt);  
    }else
    {
        printf("can not allocated buffers\n");
        return -1;
    }

     for (i = 0; i < buf_cnt; ++i)
    {
        memset(&cam->buf,0,sizeof(cam->buf));
        cam->buf.index = i;
        cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cam->buf.memory = V4L2_MEMORY_MMAP;
        /*
        * 将该 buffer 放入驱动的“可用队列”
        * 作用：
        * 1. 告诉驱动：这个 buffer 是空的，可以使用
        * 2. 驱动后续会从队列中取出 buffer，用来存放采集到的图像数据
        *
        * 注意：
        * 前面内存映射只是把驱动（内核）里的 buffer 映射到用户空间，让你能通过指针访问它
        * 这里只是“交给驱动”，并没有数据产生
        * 必须在 STREAMON 之前，把所有 buffer 都 QBUF 一次
        */
        if(ioctl(cam->fd,VIDIOC_QBUF,&cam->buf) != 0)
        {
            printf("Unabl to queue buffer\n");
            return -1;
        }
    }

    printf("queue buffers ok\n");

    return 0;

}


int camera_open(camera *cam)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    /* 启动摄像头*/
    if(0 != ioctl(cam->fd,VIDIOC_STREAMON,&type))
    {
        printf("Unable to start capture\n");
        return -1;
    }
    printf("start capture ok\n");
    return 0;
}

int camera_close(camera *cam)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    /* 启动摄像头*/
    if(0 != ioctl(cam->fd,VIDIOC_STREAMOFF,&type))
    {
        printf("Unable to stop capture\n");
        return -1;
    }
    printf("stop captre\n");
    return 0;
}



int main(int argc,char *argv[])
{
    struct pollfd fds[1];
    char filename[32];
    int file_cnt = 0;
    int ret = 0;
    int fd_file = 0;

    if(argc != 2)
    {
        printf("Usage: %s </dev/videoX>,print format for video \n",argv[0]);
        return -1;
    }

    ret = camera_open_dev(&cam,argv[1]);
    if(ret < 0)return -1;


    ret = camera_enum_formats(&cam,argv[1]);
    if(ret < 0)return -1;

    ret = camera_set_formats(&cam);
    if(ret < 0)return -1;

    ret =  camera_get_formats(&cam);
    if(ret < 0)return -1;

    ret = camera_init_buffers(&cam);
    if(ret < 0)return -1;

    ret = camera_open(&cam);
    if(ret < 0)return -1;

    while (1)
    {
        printf("run\n");

        memset(fds,0,sizeof(fds));
        fds[0].fd = cam.fd; // 设备文件描述符
        fds[0].events = POLLIN;     // 监听“可读事件”

         /*  
            &fds：监听对象
            1：监听数量
           -1：一直等待（阻塞）         
        */

       printf("run\n");
        if(1 == poll(fds,1,-1))    /* 1代表有数据*/
        {
            memset(&cam.buf,0,sizeof(cam.buf));
             cam.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            cam.buf.memory = V4L2_MEMORY_MMAP;

            /*DQBUF：取出已经完成的buffer*/
            if(ioctl(cam.fd,VIDIOC_DQBUF,&cam.buf) != 0)
            {
                printf("Unable to Dequeue buffer\n");
                return -1;
            }
            /* 把buffer的数据存为文件*/
            sprintf(filename,"video_raw_data%04d.jpeg",file_cnt++);
            
             fd_file = open(filename,O_RDWR | O_CREAT,0666);
            if(fd_file < 0)
            {
                printf("can not create file : %s",filename);
            }

            printf("capture to %s\n",filename);
            
            printf("jpeg size = %d\n",cam.buf.bytesused);

            /*第 i 个 buffer 对应的“图像数据起始地址”（用户空间指针）*/
            /* 这一帧数据实际用了多少字节*/
            write(fd_file,cam.bufs[cam.buf.index],cam.buf.bytesused);

            close(fd_file);

            /* 把buffer 放入队列*/
            if(0 != ioctl(cam.fd,VIDIOC_QBUF,&cam.buf))
            {
                printf("Unabl to queue buffer");
                return -1;
            }
            
        }else
        {
            printf("poll ret\n");
        }
        
    }

    camera_close(&cam);
    close(cam.fd);


    return 0;
}


