
import json
from optparse import OptionParser
import os

def parse_param():
    usage = "usage: %prog [input_directory] [btq]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("-i",action="store",dest="input_dir",default="",type=str,help='input_dir')
    parser.add_option("-o",action="store",dest="output_dir",default="",type=str,help='target_btq')
    options, args = parser.parse_args()
    return options, args, usage

if __name__ == '__main__':
    # 解析参数
    options, args, usage = parse_param()
    print(options, args, usage)
    input_dir = options.input_dir
    output_dir = options.output_dir
    
    for filename in os.listdir(input_dir):
        # 构建完整的文件路径
        file_path = os.path.join(input_dir, filename)
        
        # 确保是文件而不是目录
        rst = []
        if os.path.isfile(file_path):
            # 打开并读取文件
            with open(file_path, 'r', encoding='utf-8') as file:
                # 逐行读取文件内容
                # 行的格式：{"user_id": "3839639693", "product_names": ["逆袭一刻", "开局太难了", "吃不到神鲲吧", "找茬大师傅", "脑力大咖秀", "疯狂飞刀转", "一决胜负", "超能防���", "鲲之召唤", "迷你闯关"]}
                for line in file:
                    data = json.loads(line)
                    user_id = data['user_id']
                    product_names = ','.join([ele.replace('', '') for ele in data['product_names']])
                    rst.append('\t'.join([user_id, product_names]))
        out_file = os.path.join(output_dir, filename.split('.')[0]+'.txt')
        with open(out_file, 'a', encoding='utf-8') as fo:
            for r in rst:
                fo.write(r + '\n')
    # write_time = time.time()
    # write_time_cost = write_time - jiexi_time
