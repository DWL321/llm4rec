import json
import time
import requests
import datetime
import os
import argparse
import logging
import re
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
            "[%(asctime)s] [%(levelname)s] - %(message)s"
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


def get_detail(args, url="https://kml.corp.kuaishou.com/v2/gaia/api/v1/service/{}/detail?id={}"):
    url = url.format(args.service_id, args.service_id)
    headers = {
        "Kml-Project-Id": args.kml_project_id,
        "KML-Auth-Token": args.kml_auth_token
    }

    try:
        r = requests.get(url, headers=headers)
        res = r.json()
        return res
    except Exception as e:
        logger.error(e)
    return None


def get_model_version(args, url="https://kml.corp.kuaishou.com/v2/model-hub/api/v1/login-required/model/{}/versions?offset=0&limit=1&modelName={}&sort=id%2Cdesc"):
    url = url.format(args.modelhub_model_name, args.modelhub_model_name)
    headers = {
        "Kml-Project-Id": args.kml_project_id,
        "KML-Auth-Token": args.kml_auth_token
    }

    try:
        r = requests.get(url, headers=headers)
        res = r.json()
        return res["data"][0]["modelVersion"], res["data"][0]["description"]
    except Exception as e:
        logger.error(e)
    return None


def update_server(args, detail_json, MODEL_VERSION, url="https://kml.corp.kuaishou.com/v2/gaia/api/v1/service/{}/upgrade?id={}"):
    # request_json = {
    #     "operationInfo": {
    #         "operationMeta": {
    #             "targetVersion": TARGET_VERSION  # "v31.0"
    #         },
    #         "scheduleInfo": {
    #             "grayscale": {
    #                 "grayscaleQuantity": {
    #                     "num": 30,
    #                     "percent": 30,
    #                     "type": "DescribeByPercent"
    #                 }
    #             }
    #         }
    #     },
    #     "serviceConfig": {
    #         "version": VERSION,  # "v31.0",
    #         "basicConfig": {
    #             "ownerID": 12569,
    #             "owner": "duyong",
    #             "description": "海外llm4rec ranking例行部署"
    #         },
    #         "deployConfig": {
    #             "deploys": [
    #                 {
    #                     "clusterName": "kml-bjzey-hb1az3",
    #                     "resourceQueueID": 2267,
    #                     "resourceQueueName": "【业务】商业化大模型-海外模型",
    #                     "cpuLabels": {
    #                         "ad-hw-big-gpu": "owner"
    #                     },
    #                     "gpuLabels": {
    #                         "ad-hw-big-gpu": "owner"
    #                     },
    #                     "resourceInfo": {
    #                         "customResource": {
    #                             "resourceList": [
    #                                 {
    #                                     "type": "CPU",
    #                                     "unit": "MilliCore",
    #                                     "amount": 24000
    #                                 },
    #                                 {
    #                                     "type": "Memory",
    #                                     "unit": "Megabyte",
    #                                     "amount": 100000
    #                                 },
    #                                 {
    #                                     "type": "GPU",
    #                                     "amount": 1,
    #                                     "unit": "Instance",
    #                                     "modelLimit": "4090"
    #                                 }
    #                             ]
    #                         },
    #                         "instanceNum": 72
    #                     }
    #                 }
    #             ]
    #         },
    #         "kessConfig": {
    #             "netEnv": "PRODUCTION"
    #         },
    #         "launchConfig": {
    #             "image": "registry.corp.kuaishou.com/kml-supercomputing-prod/llm-chat-service:v0.5.1-decloud-12798-20240625144705",
    #             "startCommands": [
    #                 "python3 -u /root/tokenizerservice/tokenizer_server.py --tokenizer_path $KML_MODEL_0_PATH/tokenizer --tokenizer_kwaiyii --tokenizer_vocab_size=128k --train_window_size=8192 &\nmpirun -n 1 --mca odls_base_sigkill_timeout 80 --allow-run-as-root /root/llm_service"
    #             ],
    #             "kspInfo": {
    #
    #             }
    #         },
    #         "metaConfig": {
    #             "name": "ftie-service-llm4rec-oversea-routine-ranking",
    #             "projectID": 10121,
    #             "type": "ServiceDense",
    #             "frameworkType": "FrameworkGeneral"
    #         },
    #         "modelConfig": {
    #             "models": [
    #                 {
    #                     "name": "llm4rec_oversea_routine_ranking",
    #                     "version": MODEL_VERSION  # "v1.5.1"
    #                 }
    #             ]
    #         },
    #         "runConfig": {
    #             "envVars": {
    #                 "DISPATCHER_SERVICE_KESS_NAME": "ai-platform-infer-dispatcher-redis-test",
    #                 "FMHA_ENABLE": "ON",
    #                 "KML_MODEL_0_NAME": "llm4rec_oversea_routine_ranking",
    #                 "KML_MODEL_0_VERSION": "ftie-service-llm4rec-oversea-routine-ranking",
    #                 "LOG_NAME": "iomanager",
    #                 "SYNC_LOG_KAFKA_TOPIC": "aip_llm_service_sync_log"
    #             },
    #             "ports": {
    #                 "useAutoPort": True
    #             },
    #             "useHostNetwork": True
    #         },
    #         "logStorageConfig": {
    #             "logVolume": {
    #                 "subPath": "",
    #                 "resourceLimit": {},
    #                 "kmlStorageData": {
    #                     "id": 0,
    #                     "name": "",
    #                     "parameter": {
    #                         "monitors": "",
    #                         "user": "",
    #                         "keyring": "",
    #                         "image": "",
    #                         "path": ""
    #                     },
    #                     "type": "",
    #                     "status": ""
    #                 },
    #             }
    #         }
    #     }
    # }
    url = url.format(args.service_id, args.service_id)
    headers = {
        "Kml-Project-Id": args.kml_project_id,
        "KML-Auth-Token": args.kml_auth_token,
        "Content-Type": "application/json"
    }

    deploys = []
    for val in detail_json["deployConfig"]["deploys"]:
        if "workloadType" in val.keys():
            val.pop("workloadType")
        deploys.append(val)
    launchConfig = detail_json["launchConfig"]
    if "codeSource" in launchConfig.keys():
        launchConfig.pop("codeSource")

    runConfig = detail_json["runConfig"]
    if "scheduleInfo" in runConfig.keys():
        runConfig.pop("scheduleInfo")
    request_json = {
        "operationInfo": {
            "operationMeta": {
                "targetVersion": detail_json["version"]  # "v31.0"
            },
            "scheduleInfo": {
                "grayscale": {
                    "grayscaleQuantity": {
                        "num": 30,
                        "percent": 30,
                        "type": "DescribeByPercent"
                    }
                }
            }
        },
        "serviceConfig": {
            "version": detail_json["version"],  # "v31.0",
            "basicConfig": detail_json["basicConfig"],
            "deployConfig": {
                "deploys": deploys,
            },
            "kessConfig": detail_json["kessConfig"],
            "launchConfig": launchConfig,
            "metaConfig": detail_json["metaConfig"],
            "modelConfig": {
                "models": [
                    {
                        "name": detail_json["modelConfig"]["models"][0]["name"],
                        "version": MODEL_VERSION  # "v1.5.1"
                    }
                ]
            },
            "runConfig": runConfig,
            "logStorageConfig": {
                "logVolume": {
                    "kmlStorageData": detail_json["logStorageConfig"]["logVolume"]["kmlStorageData"],
                    "subPath": detail_json["logStorageConfig"]["logVolume"]["subPath"],
                }
            }
        }
    }


    try:
        r = requests.post(url, headers=headers, json=request_json)
        res = r.json()
        return res
    except Exception as e:
        logger.error(e)
    return None


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


# 判断hf_bin格式是否已经转为ftie格式
def check_ftie_is_convert(args, day_time):
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
    # check_file_path = args.model_save_path.format(day_time) + "/save/{}/ftie/SUCCESS".format(iter_path)
    # month_day = day_time[4: ]
    # check_file_path = args.ftie_model_path.format(month_day, month_day, month_day) + "/SUCCESS"
    check_file_path = args.ftie_model_path + "/{}/{}_0/v{}/SUCCESS".format(day_time, day_time, day_time)
    logger.info("ftie检查地址: {}".format(check_file_path))

    if os.path.exists(check_file_path):
        return True
    else:
        return False


# 检查模型是否上传了
def check_modelhup_upload(args, day_time):
    month_day = day_time[4:]
    # check_path = args.ftie_model_path.format(month_day, month_day, month_day) + "/_UPSUCCESS"
    check_path = args.ftie_model_path + "/{}/{}_0/v{}/_UPSUCCESS".format(day_time, day_time, day_time)

    if os.path.exists(check_path) is False:
        return False
    else:
        return True

def write_modelhub_upload_flag(args, day_time):
    month_day = day_time[4:]
    check_path = args.ftie_model_path + "/{}/{}_0/v{}/_UPSUCCESS".format(day_time, day_time, day_time)
    with open(check_path, "w", encoding="utf8") as f_out:
        pass


# hfbin模型转ftie
def hfbin_2_ftie(args, day_time):
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


    input_path = args.model_save_path + "/{}/{}/save/{}/hf_bin".format(args.exp_name, day_time, iter_path)
    # output_path = args.model_save_path.format(day_time) + "/save/{}/ftie".format(iter_path)
    # month_day = day_time[4:]
    output_path = args.ftie_model_path + "/{}/{}_0/v{}".format(day_time, day_time, day_time)
    if os.path.exists(output_path) is False:
        os.makedirs(output_path)

    code_path = "/mmu_vcg_wjc_hdd/duyong/code/ftie-weight-convert/python/hf2infer_framework.py"

    run_shell = "bash {}/ftie_weight_convert/hf2infer_framework.sh {} {} {}".format(current_work_dir, input_path, output_path, code_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)


# 复制文件
def copy_base_file(args, day_time):
    # month_day = day_time[4:]
    output_path = args.ftie_model_path + "/{}/{}_0/v{}".format(day_time, day_time, day_time)

    # base_file_path = "/mmu_vcg_wjc_hdd/duyong/tmp/modelhub/10121/llm4rec_oversea_routine_ranking/model/1/240704_0/v0701"
    base_file_path = args.ftie_model_path + "/{}/{}_0/v{}".format("0", "0", "0")
    run_shell = "cp -r {}/config.ini {}/".format(base_file_path, output_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)

    run_shell = "cp -r {}/tokenizer {}/".format(base_file_path, output_path)
    logger.info("\t执行脚本：{}".format(run_shell))
    os.system(run_shell)


def upload_model_to_modelhub(args, day_time):
    month_day = day_time[4:]
    output_path = args.ftie_model_path + "/{}/{}_0/v{}".format(day_time, day_time, day_time)

    run_shell = "mhcli --new-minor -p {} -u {} -n {} --description {}".format(output_path, args.modelhub_model_user, args.modelhub_model_name, day_time)
    logger.info("\t执行脚本：{}".format(run_shell))
    result = os.popen(run_shell)
    res = result.read()
    res_string = ""
    for line in res.splitlines():
        res_string += line
    logger.info("\t结果：{}".format(res_string))


def check_and_update_server(args, day_time):
    detail_json = get_detail(args)
    if detail_json is None:
        return False
    now_version = detail_json["modelConfig"]["models"][0]["version"]
    logger.info("\tnow_version: {}".format(now_version))
    new_model_version, description = get_model_version(args)
    if new_model_version is None:
        return False
    # if description == day_time:
    #     return True
    logger.info("\tnew_model_version: {}, description: {}".format(new_model_version, description))
    if new_model_version == now_version:
        return True
    TARGET_VERSION = detail_json["version"]
    VERSION = detail_json["version"]
    MODEL_VERSION = new_model_version
    logger.info("\tversion: {}, model_version: {}".format(VERSION, MODEL_VERSION))
    update_flag = update_server(args, detail_json, MODEL_VERSION)
    if update_flag is None:
        return False
    logger.info("\tupdate state: {}".format(json.dumps(update_flag, ensure_ascii=False)))
    return True


def get_recent_model_day(args, day_time):
    model_base_path = args.model_save_path + "/{}".format(args.exp_name)
    model_paths = os.listdir(model_base_path)
    model_paths.sort(reverse=True)
    pre_day = model_paths[0]
    pre_day2 = model_paths[0]
    if len(model_paths) > 1:
        pre_day2 = model_paths[1]
    return pre_day, pre_day2


def main(args, pre_day):
    logger.info("当前, pre_data: {}==========".format(pre_day))
    if check_hfbin_is_convert(args, pre_day) is False:
        logger.info("hf_bin模型转换未完成")
    else:
        logger.info("hf_bin模型转换完成")

        if check_ftie_is_convert(args, pre_day) is False:
            logger.info("ftie模型转换开始")
            hfbin_2_ftie(args, pre_day)
        else:
            logger.info("ftie模型转换完成")
            if check_modelhup_upload(args, pre_day) is False:
                logger.info("模型上传未完成")
                logger.info("开始复制文件")
                copy_base_file(args, pre_day)
                logger.info("开始注册模型")
                upload_model_to_modelhub(args, pre_day)
                logger.info("开始检查线上版本")
                update_flag = check_and_update_server(args, pre_day)
                if update_flag:
                    write_modelhub_upload_flag(args, pre_day)
            else:
                logger.info("模型上传完成")



if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--local_data_path",
        default="/mmu_vcg_wjc_hdd/duyong/llm4rec_data",
        type=str, required=False,
        help="本地数据保存地址"
    )
    parser.add_argument(
        "--model_save_path",
        default="/mmu_vcg_wjc_hdd/duyong/llm4rec_model",
        type=str,
        required=False,
        help="本地模型保存地址"
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
        '--modelhub_model_name',
        default="llm4rec_oversea_routine_retrival",
        type=str,
        required=False,
        help="model_hub模型名字"
    )
    parser.add_argument(
        '--modelhub_model_user',
        default="duyong",
        type=str,
        required=False,
        help="model_hub模型拥有者"
    )
    parser.add_argument(
        '--service_name',
        default="ftie-service-llm4rec-oversea-routine-retrival",
        type=str,
        required=False,
        help="部署的服务名字"
    )
    parser.add_argument(
        '--service_id',
        default="212869",
        type=str,
        required=False,
        help="部署的服务id"
    )
    parser.add_argument(
        '--kml_project_id',
        default="10121",
        type=str,
        required=False,
        help="kml资源组id"
    )
    parser.add_argument(
        '--kml_auth_token',
        default="UZvuots6IrQhYrkOCmfUrBF8oP0GNQEbZUkB2o8mP14KeMTPxoWxYXzLVDCgXbIj",
        type=str,
        required=False,
        help="kml自己的token"
    )
    # /mmu_vcg_wjc_hdd/duyong/tmp/modelhub/10121/llm4rec_oversea_routine_ranking/model/{}/24{}_0/v{}
    parser.add_argument(
        '--ftie_model_path',
        default="/mmu_vcg_wjc_hdd/duyong/modelhub",
        type=str,
        required=False,
        help="model_hub模型保存位置"
    )

    args = parser.parse_args()
    args.ftie_model_path = args.ftie_model_path + "/{}/{}/model".format(args.kml_project_id, args.modelhub_model_name)

    logger = setup_log("ftie_convert_{}".format(args.exp_name))

    while True:
        try:
            pre_day = datetime.datetime.now() + datetime.timedelta(days=-1)
            pre_day = pre_day.strftime('%Y%m%d')
            # pre_day2 = datetime.datetime.now() + datetime.timedelta(days=-2)
            # pre_day2 = pre_day2.strftime('%Y%m%d')

            logger.info("==========例行检查开始, pre_data: {}==========".format(pre_day))
            pre_day, pre_day2 = get_recent_model_day(args, pre_day)
            main(args, pre_day2)
            main(args, pre_day)
            time.sleep(600)
        except Exception as e:
            logger.error(e)







