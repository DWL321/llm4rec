import json
import time
import datetime
import os
import argparse
import logging
import re
import shutil
import tqdm
from logging.handlers import TimedRotatingFileHandler


def setup_log(log_name):
    logger = logging.getLogger(log_name)
    current_work_dir = os.getcwd()
    log_path = os.path.join(current_work_dir + "/logs", log_name)
    if not os.path.exists(current_work_dir + "/logs"):
        os.makedirs(current_work_dir + "/logs")

    logger.setLevel(logging.INFO)
    file_handler = TimedRotatingFileHandler(
        filename=log_path, when="MIDNIGHT", interval=1, backupCount=30
    )
    file_handler.suffix = "%Y-%m-%d.log"
    file_handler.extMatch = re.compile(r"^\d{4}-\d{2}-\d{2}.log$")
    file_handler.setFormatter(
        logging.Formatter(
            "[%(asctime)s] [%(levelname)s] - (%(filename)s:%(lineno)d) - %(message)s"
        )
    )

    sh = logging.StreamHandler()  # 往屏幕上输出
    sh.setFormatter(
        logging.Formatter(
            "[%(asctime)s] [%(levelname)s] - %(message)s"
        )
    )

    logger.addHandler(file_handler)
    logger.addHandler(sh)

    return logger


# 判断数据是否准备好
def check_data_is_valid(args, day_time):
    check_hdfs_path = args.remote_data_path + "{}".format(day_time)
    run_shell = "hdfs dfs -ls " + check_hdfs_path
    result = os.popen(run_shell)
    res = result.read()
    data_flag = False
    for line in res.splitlines():
        if "_SUCCESS" in line:
            data_flag = True
            break
    return data_flag

# 判断数据是否上传
def check_data_is_upload(args, day_time):
    check_hdfs_path = args.upload_data_path + "/{}".format(day_time)

    run_shell = "hdfs dfs -ls " + check_hdfs_path
    logger.info("\t执行脚本：{}".format(run_shell))
    result = os.popen(run_shell)
    res = result.read()
    data_flag = False
    success_flag = False
    for line in res.splitlines():
        if "product_result.json" in line:
            data_flag = True
        if "_SUCCESS" in line:
            success_flag = True
    if data_flag is True and success_flag is False:
        run_shell = "export HADOOP_USER_NAME=mmu; hdfs dfs -touchz {}/_SUCCESS".format(check_hdfs_path)
        logger.info("\t执行脚本：{}".format(run_shell))
        os.system(run_shell)
    return data_flag


# 判断数据是否已经下载好
def check_data_is_download(args, day_time):
    local_data = args.local_data_path + "/{}/p_date={}/_SUCCESS".format(args.exp_name, day_time)
    if os.path.exists(local_data):
        return True
    else:
        return False

# 判断要推理的文件是否准备好
def check_data_is_merge(args, day_time):
    out_path = args.local_data_path + "/{}/p_date={}/product.json".format(args.exp_name, day_time)

    return os.path.exists(out_path)

# 判断结果是否生成好
def check_data_is_gen(args, day_time):
    out_path = args.local_data_path + "/{}/p_date={}/product_result.json".format(args.exp_name, day_time)

    return os.path.exists(out_path)


# 下载数据
def download_data(args, day_time):
    local_path = args.local_data_path + "/{}/".format(args.exp_name)
    if os.path.exists(local_path) is False:
        os.makedirs(local_path)
    remote_path = args.remote_data_path + "{}".format(day_time)
    run_shell = "hdfs dfs -get {} {}".format(remote_path, local_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)

# 上传数据
def upload_data(args, day_time):
    local_path = args.local_data_path + "/{}/p_date={}/product_result.json".format(args.exp_name, day_time)
    remote_path = args.upload_data_path + "/{}".format(day_time)
    # run_shell = "export HADOOP_USER_NAME=mmu"
    # logger.info("\t执行脚本：{}".format(run_shell))
    # os.system(run_shell)

    run_shell = "export HADOOP_USER_NAME=mmu; hdfs dfs -mkdir {}".format(remote_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)

    run_shell = "export HADOOP_USER_NAME=mmu; hdfs dfs -put {} {}/product_result.json".format(local_path, remote_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)

    run_shell = "export HADOOP_USER_NAME=mmu; hdfs dfs -touchz {}/_SUCCESS".format(remote_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)

# 准备推理数据
def merge_data(args, day_time):
    root_path = args.local_data_path + "/{}/p_date={}".format(args.exp_name, day_time)
    out_path = args.local_data_path + "/{}/p_date={}/product.json".format(args.exp_name, day_time)

    if os.path.exists(out_path):
        return True
    try:
        file_names = []
        for file_name in os.listdir(root_path):
            if "part" in file_name:
                file_names.append(args.local_data_path + "/{}/p_date={}/{}".format(args.exp_name, day_time, file_name))
        # 文件排序
        data_count = 0
        with open(out_path, "w", encoding="utf8") as f_out:
            for file_name in tqdm.tqdm(file_names):
                with open(file_name, encoding="utf8") as f_in:
                    for line in f_in.readlines():
                        if line.endswith("\n"):
                            f_out.write(line)
                        else:
                            f_out.write(line + "\n")
                        data_count += 1

        logger.info("\t收到数据量：{}".format(data_count))
        return True
    except Exception as e:
        return False

def deal_unused_data(args, day_time):
    watch_day = datetime.datetime.strptime(day_time, '%Y%m%d') + datetime.timedelta(days=-(args.model_max_delay_day + 2))
    watch_day = watch_day.strftime('%Y%m%d')

    data_path = args.local_data_path + "/{}/p_date={}/".format(args.exp_name, watch_day)
    if os.path.exists(data_path):
        logger.info("\t删除数据: {}".format(data_path))
        shutil.rmtree(data_path)


def generate(args, day_time):
    input_path = args.local_data_path + "/{}/p_date={}/product.json".format(args.exp_name, day_time)
    output_path = args.local_data_path + "/{}/p_date={}/product_result.json".format(args.exp_name, day_time)

    prompt1 = """你是一位产品专家，拥有丰富的互联网软件产品、教育类产品、金融类产品等等相关的广博且深厚的知识，请你从不同的角度刻画以下产品的特征：{}"""
    prompt2 = """你是一位产品专家，拥有丰富的互联网软件产品、教育类产品、金融类产品等等相关的广博且深厚的知识，我会给你产品名、产品所属的行业信息，请你从不同的角度刻画产品的特征，你需要刻画的产品信息如下：
产品名为：{}，所属一级行业为：{}，所属二级行业为：{}

请结合你自身丰富的世界知识从不同的角度刻画这个产品的特征。
"""
    products = []
    fcs = []
    scs = []
    texts = []
    fc_sc = False
    line_idx = 0
    with open(input_path, encoding="utf8") as f_in:
        for line in f_in.readlines():
            try:
                line = json.loads(line)
                if line_idx == 0:
                    if "fc" in line.keys():
                        fc_sc = True
                    else:
                        fc_sc = False
                    line_idx += 1
                if fc_sc is False:
                    product = line["product"]
                else:
                    product = line["product_name"]
                    fc = line["fc"]
                    sc = line["sc"]
            except Exception as e:
                continue
            products.append(product)
            if fc_sc is True:
                fcs.append(fc)
                scs.append(sc)
                question = prompt2.format(product, fc, sc)
            else:
                question = prompt1.format(product)
            messages = [
                {"role": "system", "content": "You are a helpful assistant."},
                {"role": "user", "content": question}
            ]
            text = tokenizer.apply_chat_template(
                messages,
                tokenize=False,
                add_generation_prompt=True
            )
            texts.append(text)

    outputs = model.generate(texts, sampling_params)
    answers = []
    for output in outputs:
        answers.append(output.outputs[0].text)
    with open(output_path, "w", encoding="utf8") as f_out:
        if fc_sc is False:
            for product, answer in zip(products, answers):
                write_json = {
                    "product": product,
                    "answer": answer
                }
                f_out.write(json.dumps(write_json, ensure_ascii=False) + "\n")
        if fc_sc is True:
            for product, answer, fc, sc in zip(products, answers, fcs, scs):
                write_json = {
                    "product": product,
                    "answer": answer,
                    "fc": fc,
                    "sc": sc
                }
                f_out.write(json.dumps(write_json, ensure_ascii=False) + "\n")


def test_generate():
    questions = ["今天天气怎么样", "你快乐吗"]
    texts = []
    for question in questions:
        messages = [
            {"role": "system", "content": "You are a helpful assistant."},
            {"role": "user", "content": question}
        ]
        text = tokenizer.apply_chat_template(
            messages,
            tokenize=False,
            add_generation_prompt=True
        )
        texts.append(text)
    outputs = model.generate(texts, sampling_params)
    answers = []
    for output in outputs:
        answers.append(output.outputs[0].text)
    print(answers)

# 获取当前位置下最新p_data或者最接近deal_day的p_date
def get_recent_upload_pdate(args, hdfs_path):
    run_shell = "hdfs dfs -ls " + hdfs_path
    logger.info("\t执行脚本：{}".format(run_shell))
    result = os.popen(run_shell)
    res = result.read()
    dates = []
    for line in res.splitlines():
        if hdfs_path in line:
            now_date = int(line.split(hdfs_path+"/")[1].replace("\n", ""))
            dates.append(now_date)

    if len(dates) == 0:
        return None
    now_date = max(dates)
    find_flag = True
    while check_data_is_upload(args, str(now_date)) is False:
        dates.remove(now_date)
        if len(dates) == 0:
            find_flag = False
            break
        else:
            now_date = max(dates)
    if find_flag is True:
        return now_date
    else:
        return None


def get_recent_remote_pdate(args, hdfs_path, deal_day=None):
    hdfs_path = hdfs_path.replace("p_date=", "")
    run_shell = "hdfs dfs -ls " + hdfs_path
    logger.info("\t执行脚本：{}".format(run_shell))
    result = os.popen(run_shell)
    res = result.read()
    dates = []
    for line in res.splitlines():
        if "p_date=" in line:
            now_date = int(line.split("p_date=")[1].replace("\n", ""))
            dates.append(now_date)
    if len(dates) == 0:
        return None
    if deal_day is None:
        return min(dates)
    else:
        min_date = None
        for val in dates:
            if val > deal_day:
                if min_date is None:
                    min_date = val
                else:
                    if val - deal_day < min_date - deal_day:
                        min_date = val
        return min_date

def deal_one_exp(args, now_remote_data_path, now_upload_data_path, now_exp_name):
    logger.info("当前实验: {}".format(now_exp_name))
    args.remote_data_path = now_remote_data_path
    args.upload_data_path = now_upload_data_path
    args.exp_name = now_exp_name

    # 获取当前处理的位置/当前位置最近的日期
    uploaded_date = get_recent_upload_pdate(args, now_upload_data_path)
    need_deal_date = get_recent_remote_pdate(args, now_remote_data_path, uploaded_date)
    logger.info("当前已经上传的位置: {}".format(str(uploaded_date) if uploaded_date is not None else "None"))
    if uploaded_date is not None:
        logger.info("\t检查删除旧数据")
        deal_unused_data(args, str(uploaded_date))
    logger.info("当前业务方上传的待处理最近位置: {}".format(need_deal_date if need_deal_date is not None else "None"))
    deal_date = None
    if uploaded_date is None and need_deal_date is None:
        pass
    elif uploaded_date is None and need_deal_date is not None:
        deal_date = need_deal_date
    elif uploaded_date is not None and need_deal_date is None:
        pass
    elif uploaded_date is not None and need_deal_date is not None:
        if need_deal_date > uploaded_date:
            deal_date = need_deal_date
    if deal_date is None:
        logger.info("上游数据没有准备好")
        return False
    deal_date = str(deal_date)
    logger.info("\t============== 开始处理, deal_day: {}============".format(deal_date))
    # 判断数据是否上传
    if check_data_is_upload(args, deal_date) is True:
        logger.info("\t当前流程处理完成")
        return False

    # 判断文件是否准备好，准备好就下一步
    if check_data_is_valid(args, deal_date):
        logger.info("\t上游数据准备完成")
    else:
        logger.info("\t上游数据准备未完成")
        return False

    # 判断文件是否下载，没下载就下载
    if check_data_is_download(args, deal_date):
        logger.info("\t数据下载完成")
    else:
        logger.info("\t开始下载数据")
        download_data(args, deal_date)
        if check_data_is_download(args, deal_date) is False:
            return True

    # 数据合并
    if check_data_is_merge(args, deal_date):
        logger.info("\t数据合并完成")
    else:
        logger.info("\t开始合并数据")
        merge_data(args, deal_date)
        if check_data_is_merge(args, deal_date) is False:
            return True

    # 判断结果是否已经生成
    if check_data_is_gen(args, deal_date):
        logger.info("\t结果生成完成")
    else:
        logger.info("\t开始推理结果")
        generate(args, deal_date)
        if check_data_is_gen(args, deal_date) is False:
            return True

    # 上传数据
    logger.info("\t开始上传数据")
    upload_data(args, deal_date)
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--gpu_number",
        default="1",
        type=str, required=False,
        help="gpu编号"
    )
    parser.add_argument(
        "--local_data_path",
        default="/share/lijupan/llm4rec_routing/llm4rec_data",
        type=str, required=False,
        help="本地数据保存地址"
    )
    parser.add_argument(
        "--remote_data_path",
        default=[
            "/home/model_strategy_e/dw/model_strategy_e.db/outerloop_daily_increacing_product/p_date=",
            "viewfs://hadoop-lt-cluster/home/outer_recall/dw/outer_recall_dev.db/outerloop_product_industry_inc/p_date="
        ],
        type=list,
        required=False,
        help="业务方给的数据地址，hdfs数据存放格式为 数据地址/日期/"
    )

    parser.add_argument(
        "--upload_data_path",
        default=[
            "/home/mmu/duyong/llm4rec/outer_loop/product2intro",
            "/home/mmu/duyong/llm4rec/outer_loop/product2intro_with_industry"
        ],
        type=list,
        required=False,
        help="上传地址，hdfs数据存放格式为 数据地址/日期/"
    )
    parser.add_argument(
        "--exp_name",
        default=[
            "product2info_outerloop",
            "product2info_outerloop_industry"
        ],
        type=list,
        required=False,
        help="实验名，构建文件夹时会 模型地址/实验名/时间"
    )

    parser.add_argument(
        '--model_max_delay_day',
        default=5,
        type=int,
        required=False,
        help="模型最长delay天数，如果delay天数之内都没有新模型，就用默认模型"
    )
    parser.add_argument(
        '--pretrain_model_path',
        default="/share/lijupan/llm4rec_routing/plms/Qwen2-7B-Instruct", type=str,
        required=False,
        help="默认预训练模型位置"
    )


    args = parser.parse_args()
    gpu_number = args.gpu_number

    os.environ["CUDA_VISIBLE_DEVICES"] = gpu_number
    from vllm import LLM, SamplingParams
    from transformers import AutoModelForCausalLM, AutoTokenizer
    sampling_params = SamplingParams(
        temperature=0.7,
        top_p=0.8,
        repetition_penalty=1.05,
        max_tokens=1024
    )
    model = LLM(model=args.pretrain_model_path, dtype="float16", max_model_len=2048)
    tokenizer = AutoTokenizer.from_pretrained(args.pretrain_model_path)
    test_generate()
    # model = AutoModelForCausalLM.from_pretrained(
    #     args.pretrain_model_path,
    #     torch_dtype="auto",
    #     device_map="auto"
    # )

    logger = setup_log("model_train_{}".format("_".join(args.exp_name)))

    # # 创建文件夹
    remote_data_paths = args.remote_data_path
    upload_data_paths = args.upload_data_path
    exp_names = args.exp_name
    for idx in range(len(exp_names)):
        data_base_path = args.local_data_path + "/{}/".format(exp_names[idx])
        if os.path.exists(data_base_path) is False:
            os.makedirs(data_base_path)

    while True:
        try:
            now_day = datetime.datetime.now()
            now_day = now_day.strftime('%Y%m%d')  # 格式化输出
            logger.info("===========例行检查开始, now_day: {}============".format(now_day))
            deal_flag = False
            for idx in range(len(remote_data_paths)):
                now_remote_data_path = remote_data_paths[idx]
                now_upload_data_path = upload_data_paths[idx]
                now_exp_name = exp_names[idx]
                now_flag = deal_one_exp(args, now_remote_data_path, now_upload_data_path, now_exp_name)
                if now_flag is True:
                    deal_flag = True
            if deal_flag is False:
                time.sleep(300)

        except Exception as e:
            logger.error(e)

