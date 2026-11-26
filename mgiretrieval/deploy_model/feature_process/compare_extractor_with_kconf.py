import json

def parse_extractor_from_kconf(in_kconf_file):
    # input_filename = 'kconf.json'
    # output_filename = 'extracted_features.txt'

    # 读取JSON文件
    with open(in_kconf_file, 'r') as file:
        data = json.load(file)

    # 获取ad_field_extractor中的特征抽取类
    extractor_classes = data['ad_field']['ad_field_extractor']
    return extractor_classes

def parse_extractor_from_feature_txt(in_feature_file):
    # 打开原始特征文件进行读取
    with open(in_feature_file, 'r') as file:
        lines = file.readlines()

    # 处理每一行，只有包含class=且不以#开头的行才添加emb_size=64
    class_lst = []
    for line in lines:
        # 检查行是否包含class=且不以#开头
        line = line.replace(' ', '')
        if 'class=' in line and not line.strip().startswith('#'):
            class_ = line.split(',')[0].split('=')[1]
            class_lst.append(class_)
    return class_lst

# 假设你的JSON文件名为data.json
in_kconf_file = 'kconf.json'
in_feature_file = 'kai_feature.txt'

kconf_extractor_classes = parse_extractor_from_kconf(in_kconf_file)
class_lst = parse_extractor_from_feature_txt(in_feature_file)
# print(class_lst[0:10])
# print(kconf_extractor_classes[0:10])
unique_classes = [cls for cls in class_lst if cls not in kconf_extractor_classes]

print(unique_classes)
unique_output_filename = './to_add_extracted_feat.txt'
with open(unique_output_filename, 'w') as file:
    for extractor in unique_classes:
        file.write('"'+extractor + '",' + '\n')

# # 将特征抽取类写入新文件
# kconf_output_filename = './kconf_extracted_feat.txt'
# with open(kconf_output_filename, 'w') as file:
#     for extractor in kconf_extractor_classes:
#         file.write(extractor + '\n')

# print(f'特征抽取类已写入到 {kconf_output_filename}')

# # 将特征抽取类写入新文件
# in_output_filename = './in_extracted_feat.txt'
# with open(in_output_filename, 'w') as file:
#     for extractor in class_lst:
#         file.write(extractor + '\n')

# print(f'特征抽取类已写入到 {in_output_filename}')
        