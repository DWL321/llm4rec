import json

def extract_product_values(file_path):
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            for line in file:
                # 假设每行是一个有效的JSON字符串
                json_obj = json.loads(line)
                # 提取product字段的值
                product_name = json_obj.get('product', '')
                if product_name:
                    print(product_name)
    except FileNotFoundError:
        print(f"The file {file_path} does not exist.")
    except json.JSONDecodeError:
        print("Error decoding JSON. Please check if the file contains valid JSON.")
    except Exception as e:
        print(f"An error occurred: {e}")

# 使用脚本
if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python extract_product.py <filename>")
    else:
        filename = sys.argv[1]
        extract_product_values(filename)
