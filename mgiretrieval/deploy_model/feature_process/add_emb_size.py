# 打开原始特征文件进行读取
with open('in_feature.txt', 'r') as file:
    lines = file.readlines()

# 处理每一行，只有包含class=且不以#开头的行才添加emb_size=64
processed_lines = []
for line in lines:
    # 检查行是否包含class=且不以#开头
    if 'class=' in line and not line.strip().startswith('#'):
        if 'dense_' in line:
            processed_lines.append(line)
        else:
            processed_lines.append(line.strip() + ',emb_size=64\n')

# 将处理后的内容写入新文件
with open('out_feature.txt', 'w') as file:
    file.writelines(processed_lines)