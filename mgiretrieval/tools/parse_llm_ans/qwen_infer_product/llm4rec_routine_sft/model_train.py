
import time
import datetime
import os
import argparse
import logging
import tqdm
import re
import shutil
from logging.handlers import TimedRotatingFileHandler


def setup_log(log_name):
    logger = logging.getLogger(log_name)
    current_work_dir = os.getcwd()
    log_path = os.path.join(current_work_dir + "/logs", log_name)

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
    check_hdfs_path = args.remote_data_path + "/{}".format(day_time)
    run_shell = "hdfs dfs -ls " + check_hdfs_path
    result = os.popen(run_shell)
    res = result.read()
    data_flag = False
    for line in res.splitlines():
        if "_SUCCESS" in line:
            data_flag = True
            break
    return data_flag


# 判断数据是否已经下载好
def check_data_is_download(args, day_time):
    local_data = args.local_data_path + "/{}/{}/_SUCCESS".format(args.exp_name, day_time)
    if os.path.exists(local_data):
        return True
    else:
        return False


# 判断是否tokenizer
def check_data_is_tokenizer(args, day_time):
    base_path = args.local_data_path + "/{}/{}/merge_train.json".format(args.exp_name, day_time)
    file_exist = True
    if os.path.exists(base_path + "_input_ids.bin") is False:
        file_exist = False
    if os.path.exists(base_path + "_labels_mask.bin") is False:
        file_exist = False
    if os.path.exists(base_path + "_sample_index.bin") is False:
        file_exist = False
    if os.path.exists(base_path + "_sample_len.bin") is False:
        file_exist = False
    return file_exist


# 判断数据是否已经合并
def check_data_is_merge(args, day_time):
    out_path = args.local_data_path + "/{}/{}/merge_train.json".format(args.exp_name, day_time)

    return os.path.exists(out_path)


# 判断是否已经训练好了
def check_model_is_trained(args, day_time):
    model_base_path = args.model_save_path + "/{}/{}/save".format(args.exp_name, day_time)
    if os.path.exists(model_base_path) is False:
        return False
    file_paths = os.listdir(model_base_path)
    iter_path = None
    for file_path in file_paths:
        if "iter_" in file_path and file_path != "iter_0000000":
            iter_path = file_path
            break
    if iter_path is None:
        return False
    check_para_path = args.model_save_path + "/{}/{}/save/{}/mp_rank_0{}_00{}/distrib_optim.pt"
    exist_flag = True
    for tp in range(4):
        for pp in range(4):
            if os.path.exists(check_para_path.format(args.exp_name, day_time, iter_path, str(tp), str(pp))) is False:
                exist_flag = False
                break
    return exist_flag


# 判断megatron格式是否已经转为hf_bin格式
def check_hfbin_is_convert(args, day_time):
    model_base_path = args.model_save_path + "/{}/{}/save".format(args.exp_name, day_time)
    if os.path.exists(model_base_path) is False:
        return False
    file_paths = os.listdir(model_base_path)
    iter_path = None
    for file_path in file_paths:
        if "iter_" in file_path and file_path != "iter_0000000":
            iter_path = file_path
            break
    if iter_path is None:
        return False
    check_para_path = args.model_save_path + "/{}/{}/save/{}/hf_bin/pytorch_model-000{}-of-00041.bin"
    exist_flag = True
    for idx in range(1, 42):
        pa_idx = str(idx)
        if idx < 10:
            pa_idx = "0" + str(idx)
        if os.path.exists(check_para_path.format(args.exp_name, day_time, iter_path, pa_idx)) is False:
            exist_flag = False
            break
    return exist_flag


# 下载数据
def download_data(args, day_time):
    local_path = args.local_data_path + "/{}/".format(args.exp_name)
    if os.path.exists(local_path) is False:
        os.makedirs(local_path)
    remote_path = args.remote_data_path + "/{}".format(day_time)
    run_shell = "hdfs dfs -get {} {}".format(remote_path, local_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)


# 合并数据，限制每天数据最多29w
def merge_data(args, day_time):
    root_path = args.local_data_path + "/{}/{}".format(args.exp_name, day_time)
    out_path = args.local_data_path + "/{}/{}/merge_train.json".format(args.exp_name, day_time)

    if os.path.exists(out_path):
        return True
    try:
        file_names = []
        for file_name in os.listdir(root_path):
            if ".json" in file_name:
                file_names.append(args.local_data_path + "/{}/{}/{}".format(args.exp_name, day_time, file_name))
        # 文件排序
        data_count = 0
        train_count = 0
        with open(out_path, "w", encoding="utf8") as f_out:
            for file_name in tqdm.tqdm(file_names):
                with open(file_name, encoding="utf8") as f_in:
                    for line in f_in.readlines():
                        if data_count < args.max_data_per_day:
                            train_count += 1
                            if line.endswith("\n"):
                                f_out.write(line)
                            else:
                                f_out.write(line + "\n")
                        data_count += 1

        logger.info("\t收到数据量：{}，实际训练：{}".format(data_count, train_count))
        return True
    except Exception as e:
        return False


# 数据tokenizer
def data_tokenizer(args, day_time):
    current_work_dir = os.getcwd()

    file_path = args.local_data_path + "/{}/{}/merge_train.json".format(args.exp_name, day_time)
    output_file = args.local_data_path + "/{}/{}/merge_train.json".format(args.exp_name, day_time)
    deal_code = current_work_dir + "/tokenizer/new_sft_tokenizer_single.py"
    tmp_dir = current_work_dir + "/tokenizer/cache_dir"
    max_len = args.max_len
    merge_post_process = current_work_dir + "/tokenizer/merge_splited_and_post_process.py"
    run_shell = "bash {}/tokenizer/split_and_parallel.sh {} {} {} {} {} {}".format(current_work_dir, file_path, output_file, deal_code, tmp_dir, max_len, merge_post_process)

    logger.info("\t执行脚本：{}".format(run_shell))
    result = os.system(run_shell)
    if result != 0:
        return False
    else:
        return True  # 结果为0则表示执行成功，为其他值则表示执行不成功


def model_train(args, day_time, pre_train_model_path):
    train_data_path = args.local_data_path + "/{}/{}/merge_train.json".format(args.exp_name, day_time)
    current_work_dir = os.getcwd()

    run_shell = "kai_launch {}/train_13b/sft_13b_8k_finetune.sh {} {} {} {} {} {}".format(current_work_dir, args.model_save_path, args.exp_name, day_time, args.tensorboard_save_path, train_data_path, pre_train_model_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)


def megatron_2_hfbin(args, day_time):
    current_work_dir = os.getcwd()
    model_base_path = args.model_save_path + "/{}/{}/save/".format(args.exp_name, day_time)
    if os.path.exists(model_base_path) is False:
        return False
    file_paths = os.listdir(model_base_path)
    iter_path = None
    for file_path in file_paths:
        if "iter_" in file_path and file_path != "iter_0000000":
            iter_path = file_path
            break
    if iter_path is None:
        return False

    run_shell = "cp {}/config.json {}".format(current_work_dir, model_base_path)

    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)

    input_path = args.model_save_path + "/{}/{}/save/{}/".format(args.exp_name, day_time, iter_path)
    output_path = input_path + "hf_bin/"
    code_path = "/mmu_vcg_wjc_hdd/duyong/code/ftie-weight-convert/python/train_framework2hf.py"

    run_shell = "bash {}/ftie_weight_convert/train_framework2hf.sh {} {} {}".format(current_work_dir, input_path, output_path, code_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)


def get_recent_checkpoints(args, day_time):
    model_base_path = args.model_save_path + "/{}".format(args.exp_name)
    recent_path = None
    recent_time = None
    day_time_day = datetime.datetime.strptime(day_time, '%Y%m%d')
    if os.path.exists(model_base_path):
        model_paths = os.listdir(model_base_path)
        for model_path in model_paths:
            if model_path == day_time:
                continue

            now_tmp_day = datetime.datetime.strptime(model_path, '%Y%m%d')
            if (day_time_day - now_tmp_day).days > args.model_max_delay_day:
                continue

            now_check_path = args.model_save_path + "/{}/{}/save/".format(args.exp_name, model_path)
            if os.path.exists(now_check_path) is False:
                continue
            file_paths = os.listdir(now_check_path)
            iter_path = None
            for file_path in file_paths:
                if "iter_" in file_path and file_path != "iter_0000000":
                    iter_path = file_path
                    break

            if iter_path is not None and check_hfbin_is_convert(args, model_path):
                if recent_path is None:
                    recent_path = now_check_path + iter_path
                    recent_time = now_tmp_day
                else:
                    if (now_tmp_day - recent_time).days > 0:
                        recent_path = now_check_path + iter_path
                        recent_time = now_tmp_day

    if recent_path is None:
        recent_path = args.pretrain_model_path
    return recent_path

def deal_unused_data(args, day_time):
    watch_day = datetime.datetime.strptime(day_time, '%Y%m%d') + datetime.timedelta(days=-(args.model_max_delay_day + 2))
    watch_day = watch_day.strftime('%Y%m%d')
    model_path = args.model_save_path + "/{}/{}/".format(args.exp_name, watch_day)
    if os.path.exists(model_path):
        logger.info("\t删除模型: {}".format(model_path))
        shutil.rmtree(model_path)
    data_path = args.local_data_path + "/{}/{}/".format(args.exp_name, watch_day)
    if os.path.exists(data_path):
        logger.info("\t删除数据: {}".format(data_path))
        shutil.rmtree(data_path)
    ftie_path = args.ftie_model_path + "/{}/".format(watch_day)
    if os.path.exists(ftie_path):
        logger.info("\t删除ftie模型: {}".format(ftie_path))
        shutil.rmtree(ftie_path)

def get_remote_recent_data_day(args, day_time):
    check_hdfs_path = args.remote_data_path
    run_shell = "hdfs dfs -ls " + check_hdfs_path
    result = os.popen(run_shell)
    res = result.read()
    result_day = None
    for line in res.splitlines():
        if check_hdfs_path in line:
            now_day = line.split(check_hdfs_path)[1].replace("\n", "").replace("/", "")
            if int(now_day) > int(day_time):
                if result_day is None:
                    result_day = now_day
                else:
                    if int(now_day) < int(result_day):
                        result_day = now_day
    if result_day is None:
        result_day = day_time
    return result_day

def get_recent_data(args, now_day):
    model_base_path = args.model_save_path + "/{}".format(args.exp_name)
    model_paths = os.listdir(model_base_path)
    if len(model_paths) == 0:
        model_paths.append(now_day)


    model_paths.sort(reverse=True)
    if check_hfbin_is_convert(args, model_paths[0]) is True:
        return get_remote_recent_data_day(args, model_paths[0])
    else:
        return model_paths[0]




if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--local_data_path",
        default="/mmu_vcg_wjc_hdd/duyong/llm4rec_data",
        type=str, required=False,
        help="本地数据保存地址"
    )
    parser.add_argument(
        "--remote_data_path",
        default="viewfs://hadoop-lt-cluster/home/ad_algo_g/users/lingyun/baiyong/llm4rec/kuaiyi/caption_train_data",
        type=str,
        required=False,
        help="业务方给的数据地址，hdfs数据存放格式为 数据地址/日期/"
    )
    parser.add_argument(
        "--model_save_path",
        default="/mmu_vcg_wjc_hdd/duyong/llm4rec_model",
        type=str,
        required=False,
        help="本地模型保存地址"
    )
    parser.add_argument(
        "--tensorboard_save_path",
        default="/mmu_vcg_wjc_hdd/duyong/llm4rec_tensorboard",
        type=str,
        required=False,
        help="tensorboard地址"
    )
    parser.add_argument(
        "--exp_name",
        default="13b_8k_retrival",
        type=str,
        required=False,
        help="实验名，构建文件夹时会 模型地址/实验名/时间"
    )
    parser.add_argument(
        '--max_len',
        default=8192,
        type=int,
        required=False,
        help="训练最大长度"
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
        default="/mmu_vcg_wjc_hdd/duyong/megatron_models/13b_i18n/retrival_caption_exp_8k/save/iter_0001896", type=str,
        required=False,
        help="默认预训练模型位置"
    )
    parser.add_argument(
        '--max_data_per_day',
        default=290000,
        type=int,
        required=False,
        help="一次增量微调最大的数据量"
    )
    parser.add_argument(
        '--ftie_model_path',
        default="/mmu_vcg_wjc_hdd/duyong/modelhub",
        type=str,
        required=False,
        help="model_hub模型保存位置"
    )
    parser.add_argument(
        '--kml_project_id',
        default="10121",
        type=str,
        required=False,
        help="kml资源组id"
    )
    parser.add_argument(
        '--modelhub_model_name',
        default="llm4rec_oversea_routine_retrival",
        type=str,
        required=False,
        help="model_hub模型名字"
    )

    args = parser.parse_args()
    args.ftie_model_path = args.ftie_model_path + "/{}/{}/model".format(args.kml_project_id, args.modelhub_model_name)


    logger = setup_log("model_train_{}".format(args.exp_name))

    model_base_path = args.model_save_path + "/{}/".format(args.exp_name)
    data_base_path = args.local_data_path + "/{}/".format(args.exp_name)
    tensorboard_base_path = args.tensorboard_save_path + "/{}/".format(args.exp_name)
    if os.path.exists(model_base_path) is False:
        os.makedirs(model_base_path)
    if os.path.exists(data_base_path) is False:
        os.makedirs(data_base_path)
    if os.path.exists(tensorboard_base_path) is False:
        os.makedirs(tensorboard_base_path)

    while True:
        try:
            now_day = datetime.datetime.now()
            now_day = now_day.strftime('%Y%m%d')  # 格式化输出

            logger.info("===========例行检查开始, now_day: {}============".format(now_day))
            pre_day = get_recent_data(args, now_day)
            logger.info("检查删除旧数据")
            deal_unused_data(args, pre_day)
            logger.info("处理日期：{}".format(pre_day))
            if check_hfbin_is_convert(args, pre_day) is True:
                logger.info("当前日期已处理完，下一天数据准备未完成")
                time.sleep(300)
                continue

            # 判断文件是否准备好，准备好就下一步
            if check_data_is_valid(args, pre_day):
                logger.info("上游数据准备完成")
            else:
                logger.info("上游数据准备未完成")
                time.sleep(300)
                continue

            # 判断文件是否下载，没下载就下载
            if check_data_is_download(args, pre_day):
                logger.info("数据下载完成")
            else:
                logger.info("开始下载数据")
                download_data(args, pre_day)
                if check_data_is_download(args, pre_day) is False:
                    continue

            # 数据合并
            if check_data_is_merge(args, pre_day):
                logger.info("数据合并完成")
            else:
                logger.info("开始合并数据")
                merge_data(args, pre_day)
                if check_data_is_merge(args, pre_day) is False:
                    continue

            # 判断是否tokenizer, 如果没有的话执行tokenizer,执行完成回到第一步
            if check_data_is_tokenizer(args, pre_day):
                logger.info("数据tokenizer完成")
            else:
                logger.info("数据tokenizer开始")
                data_tokenizer(args, pre_day)
                if check_data_is_tokenizer(args, pre_day) is False:
                    continue

            # 判断模型是否训练好，训练好就不管
            if check_model_is_trained(args, pre_day):
                logger.info("模型已经训练完成")
            else:
                logger.info("开始训练模型")
                recent_checkpoint = get_recent_checkpoints(args, pre_day)
                logger.info("\t加载预训练参数：{}".format(recent_checkpoint))
                model_train(args, pre_day, recent_checkpoint)

            # 判断模型是否转为hf_bin
            if check_hfbin_is_convert(args, pre_day):
                logger.info("模型转hf_bin完成")
            else:
                logger.info("开始转hf_bin模型")
                megatron_2_hfbin(args, pre_day)
        except Exception as e:
            logger.error(e)

