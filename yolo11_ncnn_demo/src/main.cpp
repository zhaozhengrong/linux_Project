#include "layer.h"
#include "net.h"

#if defined(USE_NCNN_SIMPLEOCV)
#include "simpleocv.h"
#else
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#endif
#include <float.h>
#include <stdio.h>
#include <vector>

struct Object {
    cv::Rect_<float> rect;  // 检测框位置和大小 (Box position & size)
    int label;              // 物体类别 (Object class)
    float prob;             // 置信度分数 (Confidence score)
};

static int detect_yolo11(cv::Mat &bgr,std::vector<Object> & objects);
static void generate_proposals_decoded(const ncnn::Mat& pred, float prob_threshold, std::vector<Object>& objects);
static void qsort_descent_inplace(std::vector<Object>& objects, int left, int right);
static void qsort_descent_inplace(std::vector<Object>& objects);

static inline float intersection_area(const Object& a, const Object& b);
static void nms_sorted_bboxes(const std::vector<Object>& objects, std::vector<int>& picked, float nms_threshold);
static cv::Mat draw_objects(const cv::Mat& bgr, const std::vector<Object>& objects);

int main(int argc, char** argv)
{
        if (argc != 2)
        {
                fprintf(stderr, "Usage: %s [imagepath]\n", argv[0]);
                return -1;
        }

         const char* imagepath = argv[1];

        /*
                1.创建一个cv;Mat对象m 
                2.把图片从磁盘读取到内存中。并由m管理这篇内存

                OpenCV 用来表示一张图片（或矩阵）的数据结构。
                m 保存了：
                图片宽度（cols）
                图片高度（rows）
                图片类型（type）
                通道数（channels）
                真正的像素数据（data）
        */
        /*
            1280 × 960 × 3
            1280 × 960 = 1,228,800 个像素
            每个像素 RGB 三个 uchar（每个 1 字节）
            总数据量：1280 × 960 × 3= 3,686,400 Byte ≈ 3.5 MB
            这就是 m.data 指向的那块内存。   
        */
       /*!!!!!!!  用 OpenCV 读取图片文件（无论什么格式：jpg、png 等） 最后都会得到BGR格式的数据*/
        cv::Mat m = cv::imread(imagepath, 1);
         if (m.empty())
        {
                fprintf(stderr, "cv::imread %s failed\n", imagepath);
                return -1;
        }

        /*创建一个可以存放很多object的动态数组*/
        std::vector<Object> objects;

        detect_yolo11(m,objects);
        
        cv::Mat result = draw_objects(m, objects);

        if (!cv::imwrite("result.jpg", result))
        {
                fprintf(stderr, "save result.jpg failed\n");
                return -1;
        }

    printf("save result.jpg success\n");

    cv::imshow("image", result);
    cv::waitKey(0);

        return 0;
}

static int detect_yolo11(cv::Mat &bgr,std::vector<Object> & objects)
{
        /* 第1步：创建网络对象  :在内存中创建一个空的神经网络容器 */
        /* Net：只加载一次，存储网络结构和参数*/
        ncnn::Net yolo11;

        /* 第2步：配置选项      启用 GPU 加速（Vulkan）*/
        yolo11.opt.use_vulkan_compute = false;
        yolo11.opt.use_local_pool_allocator = false;

        /*第3步：加载模型文件 (Load model files)*/
        yolo11.load_param("../model/model.ncnn.param");
        yolo11.load_model("../model/model.ncnn.bin");


        const int target_size = 640;
        const float prob_threshold = 0.25f;    // 置信度阈值
        const float nms_threshold = 0.45f;     // NMS 阈值

        int img_w = bgr.cols;
        int img_h = bgr.rows;

        // ultralytics/cfg/models/v8/yolo11.yaml
        /*
                YOLO11 在神经网络的三个不同深度提取特征，生成三个输出：
                小尺度检测头（stride=8）：捕获细节，检测小物体
                中尺度检测头（stride=16）：检测中等大小物体
                大尺度检测头（stride=32）：检测大物体
        */
        std::vector<int> strides(3);
        strides[0] = 8;
        strides[1] = 16;
        strides[2] = 32;
        const int max_stride = 32;

        // letterbox pad to multiple of max_stride
        /* 这里按比例缩放图片*/
        /* 
                1920×1080 → 640×360（宽压缩）
                1080×1920 → 360×640（高压缩）
                300×300 → 640×640（等比放大）
                200×400 → 320×640（等比放大
        */
        int w = img_w;
        int h = img_h;
        float scale = 1.f;
        if (w > h)
        {
                scale = (float)target_size / w;
                w = target_size;
                h = h * scale;
        }
        else
        {
                scale = (float)target_size / h;
                h = target_size;
                w = w * scale;
        }

        /*输入的图片从opencv读取之后 会变为BGR 所以这里要转换为YOLO11 可以识别的RGB 数据*/
        /*    ncnn::Mat in 输入张量 
                作用：
                将原始图像像素数据转换为网络可接受的格式
                从 BGR 颜色空间转换为 RGB
                同时进行图像缩放（从原始尺寸 img_w × img_h 缩放到 w × h）
                存储网络的输入数据        
        */
        ncnn::Mat in = ncnn::Mat::from_pixels_resize(bgr.data,ncnn::Mat::PIXEL_BGR2RGB,img_w,img_h,w,h);

         // letterbox pad to target_size rectangle

         /* 原图宽高比不变 周围用灰色（114, 114, 114）宽高填充到32的倍数*/
         /*   1920×1080	    640×360	    w: 0, h: 24	        640×384
                800×600	         640×480	 w: 0, h: 0	      640×480
                1280×720	640×360	        w: 0, h: 24	    640×384
        */
        // int wpad = (w + max_stride - 1) / max_stride * max_stride - w;    
        // int hpad = (h + max_stride - 1) / max_stride * max_stride - h;

        /* 这里宽高都会补到640*640*/
        int wpad = target_size - w;
        int hpad = target_size - h;
    
        ncnn::Mat in_pad;
         /*
            copy_make_border(
                in,                      // 输入图像 (640×360)
                in_pad,                  // 输出图像 (640×640)
                hpad / 2,                // 上方填充像素数
                hpad - hpad / 2,         // 下方填充像素数
                wpad / 2,                // 左方填充像素数
                wpad - wpad / 2,         // 右方填充像素数
                ncnn::BORDER_CONSTANT,   // 填充方式（常数）
                114.f                    // 填充值（灰色 114）
            );
        */
        ncnn::copy_make_border(in, in_pad, hpad / 2, hpad - hpad / 2, wpad / 2, wpad - wpad / 2, ncnn::BORDER_CONSTANT, 114.f);

        /* 归一化 将 0~255 → 0.0~1.0（连续浮点数）  */
        const float norm_vals[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
        in_pad.substract_mean_normalize(0, norm_vals);


        /* [输入图像] → 预处理(in_pad) → 创建提取器(ex) → 输入数据(in0) → 网络推理 → 提取结果(out0)*/

        /* Extractor：每次推理创建一个新实例，存储执行状态*/
        ncnn::Extractor ex = yolo11.create_extractor();
        printf("Extractor ok\r\n");

        /* 把640×640×3 图片放进网络输入口*/
        ex.input("in0", in_pad);
        printf("in ok\r\n");
       
        /* 
                ncnn::Mat out 输出张量 
                作用：
                接收神经网络推理的输出结果
                存储网络计算后的特征图或预测结果
                在代码中是 YOLO 的检测结果（形状为 144 × 8400 如注释所示）
        */
        ncnn::Mat out;
        ex.extract("out0", out);
        printf("w=%d h=%d c=%d dims=%d\n",
       out.w,
       out.h,
       out.c,
       out.dims);
        printf("Output ok: %d × %d\n", out.h, out.w);

        std::vector<Object> proposals;
         // ★ 关键步骤：从 out 创建新的 Object 对象，存储到 proposals
        generate_proposals_decoded(out, prob_threshold, proposals);        
        printf("out ok\r\n");

        qsort_descent_inplace(proposals);

         // apply nms with nms_threshold
        std::vector<int> picked;
        nms_sorted_bboxes(proposals, picked, nms_threshold);

         int count = picked.size();

        /* 把框的坐标恢复到之前图片的大小*/
        objects.resize(count);
        for (int i = 0; i < count; i++)
        {
                objects[i] = proposals[picked[i]];

                // adjust offset to original unpadded
                float x0 = (objects[i].rect.x - (wpad / 2)) / scale;
                float y0 = (objects[i].rect.y - (hpad / 2)) / scale;
                float x1 = (objects[i].rect.x + objects[i].rect.width - (wpad / 2)) / scale;
                float y1 = (objects[i].rect.y + objects[i].rect.height - (hpad / 2)) / scale;

                // clip
                x0 = std::max(std::min(x0, (float)(img_w - 1)), 0.f);
                y0 = std::max(std::min(y0, (float)(img_h - 1)), 0.f);
                x1 = std::max(std::min(x1, (float)(img_w - 1)), 0.f);
                y1 = std::max(std::min(y1, (float)(img_h - 1)), 0.f);

                objects[i].rect.x = x0;
                objects[i].rect.y = y0;
                objects[i].rect.width = x1 - x0;
                objects[i].rect.height = y1 - y0;
        }

        return 0;
}

static void generate_proposals_decoded(const ncnn::Mat& pred, float prob_threshold, std::vector<Object>& objects)
{
    if (pred.dims != 2 || pred.h < 5)
    {
        fprintf(stderr, "unexpected output shape: w=%d h=%d c=%d dims=%d\n",
                pred.w, pred.h, pred.c, pred.dims);
        return;
    }

    const int num_anchors = pred.w;      // 8400
    const int num_class = pred.h - 4;    // 80

    for (int i = 0; i < num_anchors; i++)
    {
        int label = -1;
        float score = -FLT_MAX;

        for (int k = 0; k < num_class; k++)
        {
            float s = pred.row(4 + k)[i];

            if (s > score)
            {
                label = k;
                score = s;
            }
        }

        if (score >= prob_threshold)
        {
            float cx = pred.row(0)[i];
            float cy = pred.row(1)[i];
            float bw = pred.row(2)[i];
            float bh = pred.row(3)[i];

            Object obj;
            obj.rect.x = cx - bw * 0.5f;
            obj.rect.y = cy - bh * 0.5f;
            obj.rect.width = bw;
            obj.rect.height = bh;
            obj.label = label;
            obj.prob = score;

            objects.push_back(obj);
        }
    }
}

static void qsort_descent_inplace(std::vector<Object>& objects, int left, int right)
{
    int i = left;
    int j = right;
    float p = objects[(left + right) / 2].prob;

    while (i <= j)
    {
        while (objects[i].prob > p)
            i++;

        while (objects[j].prob < p)
            j--;

        if (i <= j)
        {
            // swap
            std::swap(objects[i], objects[j]);

            i++;
            j--;
        }
    }

    // #pragma omp parallel sections
    {
        // #pragma omp section
        {
            if (left < j) qsort_descent_inplace(objects, left, j);
        }
        // #pragma omp section
        {
            if (i < right) qsort_descent_inplace(objects, i, right);
        }
    }
}

static void qsort_descent_inplace(std::vector<Object>& objects)
{
    if (objects.empty())
        return;

    qsort_descent_inplace(objects, 0, objects.size() - 1);
}

static inline float intersection_area(const Object& a, const Object& b)
{
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}

static void nms_sorted_bboxes(const std::vector<Object>& objects, std::vector<int>& picked, float nms_threshold)
{
    picked.clear();
    bool agnostic = false ;
    const int n = objects.size();

    std::vector<float> areas(n);
    for (int i = 0; i < n; i++)
    {
        areas[i] = objects[i].rect.area();
    }

    for (int i = 0; i < n; i++)
    {
        const Object& a = objects[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++)
        {
            const Object& b = objects[picked[j]];

            if (!agnostic && a.label != b.label)
                continue;

            // intersection over union
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            // float IoU = inter_area / union_area
            if (inter_area / union_area > nms_threshold)
                keep = 0;
        }

        if (keep)
            picked.push_back(i);
    }
}

static cv::Mat draw_objects(const cv::Mat& bgr, const std::vector<Object>& objects)
{
    static const char* class_names[] = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
        "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
        "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
        "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
        "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
        "hair drier", "toothbrush"
    };

    static cv::Scalar colors[] = {
        cv::Scalar(244, 67, 54),
        cv::Scalar(233, 30, 99),
        cv::Scalar(156, 39, 176),
        cv::Scalar(103, 58, 183),
        cv::Scalar(63, 81, 181),
        cv::Scalar(33, 150, 243),
        cv::Scalar(3, 169, 244),
        cv::Scalar(0, 188, 212),
        cv::Scalar(0, 150, 136),
        cv::Scalar(76, 175, 80),
        cv::Scalar(139, 195, 74),
        cv::Scalar(205, 220, 57),
        cv::Scalar(255, 235, 59),
        cv::Scalar(255, 193, 7),
        cv::Scalar(255, 152, 0),
        cv::Scalar(255, 87, 34),
        cv::Scalar(121, 85, 72),
        cv::Scalar(158, 158, 158),
        cv::Scalar(96, 125, 139)
    };

    cv::Mat image = bgr.clone();

    for (size_t i = 0; i < objects.size(); i++)
    {
        const Object& obj = objects[i];

        const cv::Scalar& color = colors[i % 19];

        fprintf(stderr, "%d = %.5f at %.2f %.2f %.2f x %.2f\n", obj.label, obj.prob,
                obj.rect.x, obj.rect.y, obj.rect.width, obj.rect.height);

        cv::rectangle(image, obj.rect, color);

        char text[256];
        sprintf(text, "%s %.1f%%", class_names[obj.label], obj.prob * 100);

        int baseLine = 0;
        cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

        int x = obj.rect.x;
        int y = obj.rect.y - label_size.height - baseLine;
        if (y < 0)
            y = 0;
        if (x + label_size.width > image.cols)
            x = image.cols - label_size.width;

        cv::rectangle(image, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                      cv::Scalar(255, 255, 255), -1);

        cv::putText(image, text, cv::Point(x, y + label_size.height),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
    }

    cv::imshow("image", image);
    cv::waitKey(0);
    return image;
}
