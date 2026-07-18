# YOLO11 模型转换为 NCNN

## 环境windows

```bash
pip install ultralytics
```

## 导出命令

```bash
yolo export model=yolo11n.pt format=ncnn
```

或者 Python：

```python
from ultralytics import YOLO

model = YOLO("yolo11n.pt")
model.export(format="ncnn")
```

## 导出后的文件

```
model.ncnn.param
model.ncnn.bin
metadata.yaml
```

## 为什么要转换？

- `.pt` 是 PyTorch 模型（包含网络结构 + 权重）
- NCNN 无法直接读取 `.pt`
- 必须转换成 `.param`（网络结构）和 `.bin`（权重）

## 注意事项

- export 不会重新训练模型
- 只是把模型转换成 NCNN 可以识别的格式



# 这是第一个文件
## 目录1
```bash
  sudo apt install `hello`
```

