# YOLO11 NCNN 调试问题总结

日期：2026-07-20

## 1. 最初的段错误

运行现象：

```bash
./yolo11_ncnn_demo ../images/cat.jpg
Extractor ok
in ok
w=8400 h=84 c=1 dims=2
Output ok: 84 x 8400
段错误 (核心已转储)
```

根因：

模型推理成功了，`out0` 已经拿到，真正崩溃发生在后处理函数里。

当前模型输出为：

```text
w = 8400
h = 84
c = 1
dims = 2
```

这表示输出格式是：

```text
84 = 4 个框坐标 + 80 个类别分数
8400 = 640x640 输入下的所有候选框数量
```

但一开始复制过来的例程后处理是按另一种输出格式解析的：

```cpp
reg_max_1 = 16;
strides = {8, 16, 32};
generate_proposals(out, strides, ...);
```

这套代码适合“原始检测头输出”，需要 C++ 里继续做 DFL、stride、anchor 解码。你的模型已经在 ncnn 模型内部完成了解码，所以继续用这套后处理会导致 `row_range()` 越界，最终段错误。

修正：

改用已解码输出解析：

```cpp
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
```

调用处从：

```cpp
generate_proposals(out, strides, in_pad, prob_threshold, proposals);
```

改成：

```cpp
generate_proposals_decoded(out, prob_threshold, proposals);
```

## 2. `84 x 8400` 是怎么来的

以 `640 x 640` 输入为例，YOLO 检测有三个尺度：

```text
stride 8:  640 / 8  = 80  => 80 x 80 = 6400
stride 16: 640 / 16 = 40  => 40 x 40 = 1600
stride 32: 640 / 32 = 20  => 20 x 20 = 400
```

候选框总数：

```text
6400 + 1600 + 400 = 8400
```

每个候选框输出：

```text
4 个框坐标 + 80 个类别分数 = 84
```

所以最终输出就是：

```text
84 x 8400
```

在 ncnn 里二维 Mat 打印为：

```text
w=8400 h=84 c=1 dims=2
```

也就是：

```text
w 是列数，h 是行数
```

## 3. 为什么例程代码不能直接搬

不是例程错，而是例程和模型输出格式必须配套。

常见有两类 YOLO11 ncnn 后处理：

第一类：原始 head 输出。

特点：

```text
输出还没 decode
C++ 里要做 DFL、stride、anchor 解码
常见维度类似 144 x 8400
144 = 64 + 80
64 = 4 个方向 * reg_max 16
```

适合这种代码：

```cpp
reg_max_1 = 16;
strides = {8, 16, 32};
generate_proposals(out, strides, ...);
```

第二类：已解码输出。

特点：

```text
模型内部已经做完 DFL、anchor、stride、sigmoid
输出直接是 bbox + class score
当前模型就是 84 x 8400
```

适合这种代码：

```cpp
num_anchors = pred.w;
num_class = pred.h - 4;
```

结论：

```text
预处理可以参考例程。
模型加载可以参考例程。
后处理必须看自己的 model.ncnn.param 和实际 out 形状。
```

## 4. NMS 默认参数编译错误

报错：

```text
error: default argument given for parameter 4
```

原因：

函数声明和函数定义里都写了默认参数：

```cpp
bool agnostic = false
```

C++ 默认参数只能写一次。

正确写法：

声明处写默认值：

```cpp
static void nms_sorted_bboxes(const std::vector<Object>& objects,
                              std::vector<int>& picked,
                              float nms_threshold,
                              bool agnostic = false);
```

定义处不要写默认值：

```cpp
static void nms_sorted_bboxes(const std::vector<Object>& objects,
                              std::vector<int>& picked,
                              float nms_threshold,
                              bool agnostic)
{
    ...
}
```

或者更简单，声明和定义都不写默认值，调用时传第四个参数：

```cpp
nms_sorted_bboxes(proposals, picked, nms_threshold, false);
```

## 5. `too few arguments` 编译错误

报错：

```text
error: too few arguments to function
```

原因：

声明里没有默认值：

```cpp
static void nms_sorted_bboxes(..., bool agnostic);
```

但是调用时只传了 3 个参数：

```cpp
nms_sorted_bboxes(proposals, picked, nms_threshold);
```

修正方式二选一：

方式一：声明处加默认值。

```cpp
static void nms_sorted_bboxes(const std::vector<Object>& objects,
                              std::vector<int>& picked,
                              float nms_threshold,
                              bool agnostic = false);
```

方式二：调用时补第四个参数。

```cpp
nms_sorted_bboxes(proposals, picked, nms_threshold, false);
```

## 6. `pool allocator destroyed too early`

报错：

```text
FATAL ERROR! pool allocator destroyed too early
0x... still in use
```

含义：

ncnn 的内存池要销毁了，但还有 `ncnn::Mat` 或中间对象引用着内存池分配的内存。

排查思路：

1. 先关闭 Vulkan，尤其是在虚拟机里运行时。

```cpp
yolo11.opt.use_vulkan_compute = false;
```

2. 确保 `out`、`Extractor`、`Net` 的生命周期清楚。

更稳的写法是让 `Extractor` 和 `out` 在一个小作用域里先释放：

```cpp
std::vector<Object> proposals;

{
    ncnn::Extractor ex = yolo11.create_extractor();
    ex.input("in0", in_pad);

    ncnn::Mat out;
    ex.extract("out0", out);

    generate_proposals_decoded(out, prob_threshold, proposals);
}
```

3. demo 阶段如果仍有 allocator 问题，可以尝试：

```cpp
yolo11.opt.use_local_pool_allocator = false;
```

但后面遇到 `free(): corrupted unsorted chunks` 后，重点又转回输入尺寸问题。

## 7. `free(): corrupted unsorted chunks`

报错：

```text
free(): corrupted unsorted chunks
已放弃 (核心已转储)
```

现象：

程序只打印到：

```text
Extractor ok
in ok
```

还没打印输出维度，说明崩在：

```cpp
ex.extract("out0", out);
```

根因：

你的模型内部固定用了：

```text
6400 + 1600 + 400 = 8400
```

这对应固定输入：

```text
640 x 640
```

但是原代码 pad 方式是补到 `32` 的倍数：

```cpp
int wpad = (w + max_stride - 1) / max_stride * max_stride - w;
int hpad = (h + max_stride - 1) / max_stride * max_stride - h;
```

比如 `cat.jpg` 是：

```text
1280 x 960
```

等比例缩放后是：

```text
640 x 480
```

`480` 已经是 `32` 的倍数，所以不会再补高，最终输入变成：

```text
640 x 480
```

但是模型内部 anchor/reshape 是按 `640 x 640` 固定写死的，所以输入尺寸对不上，可能导致推理内部内存错误。

修正：

把 pad 改成固定补到 `640 x 640`：

```cpp
int wpad = target_size - w;
int hpad = target_size - h;

ncnn::copy_make_border(in, in_pad,
                       hpad / 2, hpad - hpad / 2,
                       wpad / 2, wpad - wpad / 2,
                       ncnn::BORDER_CONSTANT, 114.f);
```

修正后：

```text
1280 x 960 -> 640 x 480 -> 上下各补 80 -> 640 x 640
```

最终程序成功输出：

```text
Extractor ok
in ok
w=8400 h=84 c=1 dims=2
Output ok: 84 x 8400
out ok
```

## 8. 最终关键修改清单

需要保留：

```cpp
yolo11.opt.use_vulkan_compute = false;
```

输入必须固定补到 `640 x 640`：

```cpp
int wpad = target_size - w;
int hpad = target_size - h;
```

后处理使用 decoded 版本：

```cpp
generate_proposals_decoded(out, prob_threshold, proposals);
```

NMS 后要把框映射回原图：

```cpp
float x0 = (objects[i].rect.x - (wpad / 2)) / scale;
float y0 = (objects[i].rect.y - (hpad / 2)) / scale;
float x1 = (objects[i].rect.x + objects[i].rect.width - (wpad / 2)) / scale;
float y1 = (objects[i].rect.y + objects[i].rect.height - (hpad / 2)) / scale;
```

最后裁剪到原图范围：

```cpp
x0 = std::max(std::min(x0, (float)(img_w - 1)), 0.f);
y0 = std::max(std::min(y0, (float)(img_h - 1)), 0.f);
x1 = std::max(std::min(x1, (float)(img_w - 1)), 0.f);
y1 = std::max(std::min(y1, (float)(img_h - 1)), 0.f);
```

## 9. 以后遇到类似问题的判断方法

第一步：打印输出维度。

```cpp
printf("w=%d h=%d c=%d dims=%d\n", out.w, out.h, out.c, out.dims);
```

第二步：看 `model.ncnn.param` 末尾。

如果看到固定：

```text
0=8400
```

并且输出：

```text
w=8400 h=84
```

说明这是已解码输出。

第三步：看输入尺寸。

如果模型固定输出 `8400`，输入最好固定为：

```text
640 x 640
```

第四步：不要只按例程搬后处理。

应该按实际输出决定：

```text
84 x 8400  -> decoded 后处理
144 x 8400 -> DFL + stride 后处理
```

## 10. 当前结论

今天遇到的主要问题不是 ncnn 本身坏了，也不是图片读取问题，而是两个匹配关系没对上：

```text
1. 模型输出格式 和 C++ 后处理代码不匹配
2. 模型固定输入尺寸 和 实际 in_pad 尺寸不匹配
```

把这两个地方改对之后，程序已经可以正常跑到：

```text
out ok
```

