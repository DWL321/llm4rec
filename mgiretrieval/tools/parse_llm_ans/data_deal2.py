import json, tqdm, os
from collections import Counter




def load_json(input_path):
    result_json = []
    with open(input_path, encoding="utf8") as f_in:
        for line in f_in:
            line = json.loads(line)
            result_json.append(line)
    return result_json

def is_none(text):
    if text is None or text == "" or text == "null" or text == "unknown" or text == "UNKNOWN":
        return True
    else:
        return False

def deal_profile_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
    sex_dict = {
        "M": "男",
        "F": "女"
    }
    info = info[0]
    result_dict["用户画像"] = {
        "性别": sex_dict[info["性别"]],
        "年龄": info["年龄"],
    }
    if is_none(info["学历"]) is False:
        result_dict["用户画像"]["学历"] = info["学历"]
    if is_none(info["是否已育"]) is False:
        result_dict["用户画像"]["是否已育"] = info["是否已育"]
    if is_none(info["是否已婚"]) is False:
        result_dict["用户画像"]["是否已婚"] = info["是否已婚"]
    if is_none(info["职业一级分类"]) is False:
        result_dict["用户画像"]["职业一级分类"] = info["职业一级分类"]
    if is_none(info["职业二级分类"]) is False:
        result_dict["用户画像"]["职业二级分类"] = info["职业二级分类"]
    if is_none(info["职业三级分类"]) is False:
        result_dict["用户画像"]["职业三级分类"] = info["职业三级分类"]
    

def deal_consuming_ability_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
        
    info = info[0]
    result_dict["用户经济能力"] = {}
    if is_none(info["手机机型"]) is False:
        result_dict["用户经济能力"]["手机机型"] = info["手机机型"]
    if is_none(info["手机价格"]) is False:
        result_dict["用户经济能力"]["手机价格"] = info["手机价格"]
    location = ""
    if is_none(info["常驻国家"]) is False:
        location += info["常驻国家"]
    if is_none(info["常驻省份"]) is False:
        location += info["常驻省份"]
    if is_none(info["常驻城市"]) is False:
        location += info["常驻城市"]
    if is_none(info["常驻地区"]) is False:
        location += info["常驻地区"]
    if is_none(info["常驻乡镇"]) is False:
        location += info["常驻乡镇"]
    if len(location) > 1:
        result_dict["用户经济能力"]["常驻地"] = location

    if is_none(info["城市等级"]) is False:
        result_dict["用户经济能力"]["城市等级"] = info["城市等级"]
    if is_none(info["常驻地类型"]) is False:
        result_dict["用户经济能力"]["常驻地类型"] = info["常驻地类型"]
    if is_none(info["常驻小区等级划分"]) is False:
        result_dict["用户经济能力"]["常驻小区等级划分"] = info["常驻小区等级划分"]
    if is_none(info["常驻小区价格"]) is False:
        result_dict["用户经济能力"]["常驻小区价格"] = info["常驻小区价格"]

def deal_photo_hetu_consume_info(result_dict, info):
    pass

def deal_photo_consume_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
    new_info = [val for val in info if is_none(val["一级行业"]) is False and is_none(val["二级行业"]) is False]
    new_info = sorted(new_info, key=lambda x: (int(x["有效播放次数"]), int(x["完播次数"]), int(x["收藏次数"])+int(x["分享次数"])+int(x["点赞次数"])), reverse=True)
    # top_cate = ["{}_{}".format(val["一级行业"], val["二级行业"]) for val in new_info[: 20]]
    # top_cate = "、".join(top_cate)
    result_dict["用户内容消费信息-视频(商业化映射)"] = new_info

def deal_photo_comment_content_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
    new_info = [val for val in info if is_none(val["一级类目"]) is False and is_none(val["二级类目"]) is False]
    top_cate = ["{}_{}".format(val["一级类目"], val["二级类目"]) if is_none(val["三级类目"]) is True else "{}_{}_{}".format(val["一级类目"], val["二级类目"], val["三级类目"]) for val in new_info]
    counter = Counter()
    counter.update(top_cate)
    top_cate = [{"类目名": k, "次数": v} for k,v in counter.most_common(100)]
    # if len(top_cate) == 0:
    #     return
    # top_cate = "、".join(top_cate)
    result_dict["用户视频评论内容"] = top_cate


def deal_live_hetu_consume_info(result_dict, info):
    pass

def deal_live_consume_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
    new_info = [val for val in info if is_none(val["一级行业"]) is False and is_none(val["二级行业"]) is False]
    new_info = sorted(new_info, key=lambda x: (float(x["观看时长"]), float(x["评论次数"]), float(x["点赞次数"]) + float(x["打赏次数"])), reverse=True)
    # top_cate = ["{}_{}".format(val["一级行业"], val["二级行业"]) for val in new_info[: 10]]
    # top_cate = "、".join(top_cate)
    result_dict["用户内容消费信息-直播(商业化映射)"] = new_info

def deal_live_comment_content_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
    new_info = [val for val in info if is_none(val["一级类目"]) is False and is_none(val["二级类目"]) is False]
    top_cate = ["{}_{}".format(val["一级类目"], val["二级类目"]) if is_none(val["三级类目"]) is True else "{}_{}_{}".format(val["一级类目"], val["二级类目"], val["三级类目"]) for val in new_info]

    counter = Counter()
    counter.update(top_cate)
    top_cate = [{"类目名": k, "次数": v} for k, v in counter.most_common(100)]
    # top_cate = "、".join(top_cate)
    result_dict["用户直播评论内容"] = top_cate


def deal_follow_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
    new_info = [val for val in info if is_none(val["一级行业"]) is False and is_none(val["二级行业"]) is False]
    new_info = sorted(new_info, key=lambda x: (int(x["关注博主数量"])), reverse=True)
    # print(new_info)
    # top_cate = ["{}_{}".format(val["一级行业"], val["二级行业"]) if is_none(val["三级类目"]) is True else "{}_{}_{}".format(val["一级行业"], val["二级行业"], val["三级类目"]) for val in new_info[: 10]]
    # top_cate = "、".join(top_cate)
    result_dict["用户关注类目"] = new_info

def deal_im_msg_info(result_dict, info):
    pass


def deal_trade_order_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
    info = sorted(info, key=lambda x: (
        int(x["日期"])), reverse=True)
    result_dict["用户电商支付数据"] = []

    for val in info[: 20]:
        tmp = {}
        if is_none(val["商品名称"]) is True:
            continue
        tmp["商品名称"] = val["商品名称"]
        if is_none(val["商品一级品类名称"]) is False:
            tmp["商品一级品类名称"] = val["商品一级品类名称"]
        if is_none(val["商品二级品类名称"]) is False:
            tmp["商品二级品类名称"] = val["商品二级品类名称"]
        if is_none(val["商品三级品类名称"]) is False:
            tmp["商品三级品类名称"] = val["商品三级品类名称"]
        result_dict["用户电商支付数据"].append(tmp)

def deal_trade_cart_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
    info = sorted(info, key=lambda x: (
        int(x["日期"])), reverse=True)
    result_dict["用户电商购物车数据"] = []
    for val in info[: 20]:
        tmp = {}
        if is_none(val["商品名称"]) is True:
            continue
        tmp["商品名称"] = val["商品名称"]
        if is_none(val["商品一级品类名称"]) is False:
            tmp["商品一级品类名称"] = val["商品一级品类名称"]
        if is_none(val["商品二级品类名称"]) is False:
            tmp["商品二级品类名称"] = val["商品二级品类名称"]
        if is_none(val["商品三级品类名称"]) is False:
            tmp["商品三级品类名称"] = val["商品三级品类名称"]

        result_dict["用户电商购物车数据"].append(tmp)

def deal_local_life_info(result_dict, info):
    pass

def deal_keyword_info(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    if len(info) == 0:
        return
    info = [val for val in info if is_none(val["搜索关键词"]) is False]
    info = sorted(info, key=lambda x: (
        int(x["日期"])), reverse=True)
    # info = info[: 20]
    # reversed(info[: 20])
    # result_dict["用户搜索数据"] = [val["搜索关键词"] for val in info]
    result_dict["用户搜索数据"] = info


def deal_install_applist_info(result_dict, info):
    pass

def deal_active_applist_info(result_dict, info):
    try:
        if len(info) == 0:
            return
        info = json.loads(info)
    except Exception as e:
        return

    info = sorted(info, key=lambda x: (
        int(x["日期"])), reverse=True)
    new_info = []
    for val in info:
        if val["应用名称"] not in new_info and is_none(val["应用名称"]) is False:
            new_info.append(val["应用名称"])
    info = new_info[: 20]
    reversed(info)
    result_dict["APP活跃"] = "、".join(info)

def deal_user_id(result_dict, info):
    result_dict["user_id"] = info

def deal_photo_ids(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    result_dict["photo_ids"] = info

def deal_product_names(result_dict, info):
    if is_none(info):
        return
    info = json.loads(info)
    result_dict["product_names"] = info


def deal_user(user_info):
    deal_key_func = {
        "profile_info": deal_profile_info, # 用户画像
        "consuming_ability_info": deal_consuming_ability_info, # 用户经济能力
        # "photo_hetu_consume_info": deal_photo_hetu_consume_info, # 用户内容消费信息-视频(全域) 这个没用用的商业化映射的
        "photo_consume_info": deal_photo_consume_info, # 用户内容消费信息-视频(商业化映射)
        # "live_hetu_consume_info": deal_live_hetu_consume_info, # 用户内容消费信息-直播(全域). 这个没用用的商业化映射的
        "live_consume_info": deal_live_consume_info, # 用户内容消费信息-直播(商业化映射)
        "follow_info": deal_follow_info, # 用户关注类目
        # "im_msg_info": deal_im_msg_info, # 用户商业化私信
        "photo_comment_content_info": deal_photo_comment_content_info, # 用户视频评论内容
        "live_comment_content_info": deal_live_comment_content_info,  # 用户直播评论内容
        "trade_order_info": deal_trade_order_info,  # 用户电商支付数据
        "trade_cart_info": deal_trade_cart_info,  # 用户电商购物车数据
        # "local_life_info": deal_local_life_info,  # 本地生活消费记录
        "keyword_info": deal_keyword_info,  # 用户搜索数据
        # "install_applist_info": deal_install_applist_info,  # APP安装(当前安装)
        # "active_applist_info": deal_active_applist_info,  # APP活跃(采买+联盟数据)
        "user_id": deal_user_id, # 用户编号
        "photo_ids": deal_photo_ids, # 用户转化photo编号
        "product_names": deal_product_names, # 用户转化产品名
    }

    # 除了以上没处理的还有这些字段没有处理['update_photo_info', 'poi_info', 'contact_name_rev_info', 'car_4s_info', 'ad_info', 'tag_info']
    
    result_key = {}
    for k, v in user_info.items():
        for kk, vv in deal_key_func.items():
            if kk in k:
                vv(result_key, v)
                # print(kk)
            

    return result_key


def get_user_base_info(user_result_key):
    pp = "在短视频平台上，这个用户可能的用户基础信息如下："

    if "用户画像" in user_result_key.keys():
        if "性别" in user_result_key["用户画像"].keys():
            pp += "性别{}".format(user_result_key["用户画像"]["性别"])
        if "年龄" in user_result_key["用户画像"].keys():
            pp += "，年龄{}".format(user_result_key["用户画像"]["年龄"])
        if "学历" in user_result_key["用户画像"].keys():
            pp += "，学历{}".format(user_result_key["用户画像"]["学历"])
        if "是否已婚" in user_result_key["用户画像"].keys():
            pp += "，婚姻状况{}".format(user_result_key["用户画像"]["是否已婚"])
        if "是否已育" in user_result_key["用户画像"].keys():
            pp += "，生育状态{}".format(user_result_key["用户画像"]["是否已育"])
        if "职业一级分类" in user_result_key["用户画像"].keys():
            pp += "，所属职业一级分类{}".format(user_result_key["用户画像"]["职业一级分类"])
        if "职业二级分类" in user_result_key["用户画像"].keys():
            pp += "，所属职业二级分类{}".format(user_result_key["用户画像"]["职业二级分类"])
        if "职业三级分类" in user_result_key["用户画像"].keys():
            pp += "，所属职业三级分类{}".format(user_result_key["用户画像"]["职业三级分类"])
    if "用户经济能力" in user_result_key.keys():
        if "常驻地" in user_result_key["用户经济能力"].keys():
            pp += "，常驻地区{}".format(user_result_key["用户经济能力"]["常驻地"])
        if "城市等级" in user_result_key["用户经济能力"].keys():
            pp += "，常驻地城市等级{}".format(user_result_key["用户经济能力"]["城市等级"])
        if "常驻地类型" in user_result_key["用户经济能力"].keys():
            pp += "，常驻地类型{}".format(user_result_key["用户经济能力"]["常驻地类型"])
        if "常驻小区价格" in user_result_key["用户经济能力"].keys():
            pp += "，常驻小区房价{}".format(user_result_key["用户经济能力"]["常驻小区价格"])
        if "常驻小区等级划分" in user_result_key["用户经济能力"].keys():
            pp += "，常驻小区等级{}".format(user_result_key["用户经济能力"]["常驻小区等级划分"])
        if "手机机型" in user_result_key["用户经济能力"].keys():
            pp += "，使用的手机型号{}".format(user_result_key["用户经济能力"]["手机机型"])
        if "手机价格" in user_result_key["用户经济能力"].keys():
            pp += "，手机的价格{}".format(user_result_key["用户经济能力"]["手机价格"])
    pp += "。"
    return pp


def get_user_photo_watch_info(user_result_key, with_count=False):
    if with_count is True:
        pp = "用户观看数量最多的视频类别和对应的次数为：\n{}。\n\n"
        photo_info = user_result_key["用户内容消费信息-视频(商业化映射)"]
        top_cate = ["视频分类{}_{}，有效播放次数{}次，完播次数{}次，分享点赞收藏次数{}次".format(val["一级行业"], val["二级行业"], int(val["有效播放次数"]), int(val["完播次数"]), int(val["收藏次数"])+int(val["分享次数"])+int(val["点赞次数"])) for val in photo_info[: 30]]
        if len(top_cate) == 0:
            return ""
        top_cate = "；\n".join(top_cate)
        return pp.format(top_cate)
    else:
        pp = "用户观看数量最多的视频类别从高到低排序为：{}。\n"
        photo_info = user_result_key["用户内容消费信息-视频(商业化映射)"]
        top_cate = ["{}_{}".format(val["一级行业"], val["二级行业"]) for val in photo_info[: 30]]
        if len(top_cate) == 0:
            return ""
        top_cate = "、".join(top_cate)
        return pp.format(top_cate)

def get_user_photo_comment_info(user_result_key, with_count=False):
    if with_count is True:
        pp = "用户评论次数最多的视频类别和对应的次数为：\n{}。\n\n"
        photo_info = user_result_key["用户视频评论内容"]
        top_cate = ["视频分类{}，评论次数{}次".format(val["类目名"], val["次数"]) for val in photo_info[: 30]]
        if len(top_cate) == 0:
            return ""
        top_cate = "；\n".join(top_cate)
        return pp.format(top_cate)
    else:
        pp = "用户评论次数最多的视频类别从高到低排序为：{}。\n"
        photo_info = user_result_key["用户视频评论内容"]
        top_cate = [val["类目名"] for val in photo_info[: 30]]
        if len(top_cate) == 0:
            return ""
        top_cate = "、".join(top_cate)
        return pp.format(top_cate)

def get_user_live_watch_info(user_result_key, with_count=False):
    if with_count is True:
        pp = "用户观看时长最多的直播内容类型和对应的时长为：\n{}。\n\n"
        photo_info = user_result_key["用户内容消费信息-直播(商业化映射)"]
        photo_info = [val for val in photo_info if float(val["观看时长"])> 1.0]
        top_cate = ["直播类型{}_{}，观看时长{}分钟".format(val["一级行业"], val["二级行业"], val["观看时长"]) for val in photo_info[: 30]]
        if len(top_cate) == 0:
            return ""
        top_cate = "；\n".join(top_cate)
        return pp.format(top_cate)
    else:
        pp = "用户观看时长最多的直播内容类型从高到低排序为：{}。\n"
        photo_info = user_result_key["用户内容消费信息-直播(商业化映射)"]
        photo_info = [val for val in photo_info if float(val["观看时长"])> 1.0]
        top_cate = ["{}_{}".format(val["一级行业"], val["二级行业"]) for val in photo_info[: 30]]
        if len(top_cate) == 0:
            return ""
        top_cate = "、".join(top_cate)
        return pp.format(top_cate)

def get_user_live_comment_info(user_result_key, with_count=False):
    if with_count is True:
        pp = "用户评论次数最多的直播内容类型和对应的次数为：\n{}。\n\n"
        photo_info = user_result_key["用户直播评论内容"]
        top_cate = ["直播类型{}，评论次数{}次".format(val["类目名"], val["次数"]) for val in photo_info[: 30]]
        if len(top_cate) == 0:
            return ""
        top_cate = "；\n".join(top_cate)
        return pp.format(top_cate)
    else:
        pp = "用户评论次数最多的直播内容类型从高到低排序为：{}。\n"
        photo_info = user_result_key["用户直播评论内容"]
        top_cate = [val["类目名"] for val in photo_info[: 30]]
        if len(top_cate) == 0:
            return ""
        top_cate = "、".join(top_cate)
        return pp.format(top_cate)

def get_user_follow_info(user_result_key, with_count=False):
    if with_count is True:
        pp = "用户关注的视频博主的类型与数量为：\n{}。\n\n"
        user_info = user_result_key["用户关注类目"]
        top_cate = ["博主类型{}_{}，数量{}个".format(val["一级行业"], val["二级行业"], val["关注博主数量"]) if is_none(val["三级类目"]) is True else "博主类型:{}_{}_{}，数量:{}".format(val["一级行业"], val["二级行业"], val["三级类目"], val["关注博主数量"]) for val in user_info[: 30]]
        top_cate = "；\n".join(top_cate)
        return pp.format(top_cate)
    else:
        pp = "用户关注的视频博主的类型数量从高到低排序为：{}。\n"
        user_info = user_result_key["用户关注类目"]
        top_cate = ["{}_{}".format(val["一级行业"], val["二级行业"]) if is_none(val["三级类目"]) is True else "{}_{}_{}".format(val["一级行业"], val["二级行业"], val["三级类目"]) for val in user_info[: 30]]
        top_cate = "、".join(top_cate)
        return pp.format(top_cate)

def get_user_search_info(user_result_key, with_count=False):
    pp = "用户最近搜索的内容如下：{}。\n"
    search_info = []
    for val in user_result_key["用户搜索数据"]:
        if val["搜索关键词"] not in search_info:
            search_info.append(val["搜索关键词"])
        if len(search_info) >= 30:
            break
    return pp.format("；".join(search_info))

def get_user_search_cate_info(user_result_key, with_count=False):
    if with_count is True:
        pp = "用户最近搜索的内容类别和次数为：\n{}。\n\n"
        counter = Counter()
        search_cate = [val["搜索一级垂类"] for val in user_result_key["用户搜索数据"] if is_none(val["搜索一级垂类"]) is False and val["搜索一级垂类"] not in ["其他", "其他-其他"]]
        counter.update(search_cate)
        top_cate = ["搜索内容类别{}，次数{}次".format(k, v) for k,v in counter.most_common(30)]
        if len(top_cate) == 0:
            return ""
        return pp.format("；\n".join(top_cate))
    else:
        pp = "用户最近搜索的内容类别次数从高到低排序为：{}。\n"
        counter = Counter()
        search_cate = [val["搜索一级垂类"] for val in user_result_key["用户搜索数据"] if is_none(val["搜索一级垂类"]) is False and val["搜索一级垂类"] not in ["其他", "其他-其他"]]
        counter.update(search_cate)
        top_cate = [k for k,v in counter.most_common(30)]
        if len(top_cate) == 0:
            return ""
        return pp.format("、".join(top_cate))

def get_user_buycar_info(user_result_key, with_count=False):
    pp = "用户近期加入购物车的商品信息为：\n{}。\n\n"
    item_infos = []
    for item in user_result_key["用户电商购物车数据"][: 20]:
        cate = item["商品一级品类名称"]
        if "商品二级品类名称" in item.keys():
            cate += "_{}".format(item["商品二级品类名称"])
        if "商品三级品类名称" in item.keys():
            cate += "_{}".format(item["商品三级品类名称"])
        now_p = "商品名称{}，商品分类{}".format(item["商品名称"], cate)
        item_infos.append(now_p)
    if len(item_infos) == 0:
        return ""
    return pp.format("；\n".join(item_infos))

def get_user_buy_info(user_result_key, with_count=False):
    pp = "用户近期购买的商品信息为：\n{}。\n\n"
    item_infos = []
    for item in user_result_key["用户电商支付数据"][: 20]:
        cate = item["商品一级品类名称"]
        if "商品二级品类名称" in item.keys():
            cate += "_{}".format(item["商品二级品类名称"])
        if "商品三级品类名称" in item.keys():
            cate += "_{}".format(item["商品三级品类名称"])
        now_p = "商品名称{}，商品分类{}".format(item["商品名称"], cate)
        item_infos.append(now_p)
    if len(item_infos) == 0:
        return ""
    return pp.format("；\n".join(item_infos))

def get_user_product_label(product_names):
    counter = Counter()
    counter.update(product_names)
    top_product = [k for k,v in counter.most_common(100)]
    return top_product
    

def create_prompt_label_data(input_path, with_count = False):
    max_count = [0 for _ in range(20)]
    result_json = []
    with open(input_path, encoding="utf8") as f_in:
        for line in tqdm.tqdm(f_in, total=100000):
            prompt = "你是一名短视频平台的用户兴趣预测专家，你将根据我向你提供的一名短视频用户在平台上的各种综合信息推断出他的兴趣。我会给你提供一名短视频用户在平台上的各种基础信息与行为信息，请你结合这些信息进行全方面地分析，分析给出用户可能会愿意付费的广告内容，以下是用户的基础信息和行为信息：\n\n"
    
            user_info = json.loads(line)
            user_result_key = deal_user(user_info)
            if "用户画像" not in user_result_key.keys() and "用户经济能力" not in user_result_key.keys():
                continue
            prompt += get_user_base_info(user_result_key)
            prompt += "\n\n在平台推送的短视频中，用户的视频内容偏好信息如下：\n"
            if "用户内容消费信息-视频(商业化映射)" in user_result_key.keys():
                prompt += get_user_photo_watch_info(user_result_key, with_count)
            if "用户视频评论内容" in user_result_key.keys():
                prompt += get_user_photo_comment_info(user_result_key, with_count)
            if "用户内容消费信息-直播(商业化映射)" in user_result_key.keys():
                prompt += get_user_live_watch_info(user_result_key, with_count)
            if "用户直播评论内容" in user_result_key.keys():
                prompt += get_user_live_comment_info(user_result_key, with_count)
            if "用户关注类目" in user_result_key.keys():
                prompt += get_user_follow_info(user_result_key, with_count)
                
            prompt += "\n用户在平台上搜索、购买的相关信息如下：\n"
            if "用户搜索数据" in user_result_key.keys():
                prompt += get_user_search_info(user_result_key, with_count)
            if "用户搜索数据" in user_result_key.keys():
                prompt += get_user_search_cate_info(user_result_key, with_count)
            
            if "用户电商购物车数据" in user_result_key.keys():
                prompt += get_user_buycar_info(user_result_key, with_count)
            if "用户电商支付数据" in user_result_key.keys():
                prompt += get_user_buy_info(user_result_key, with_count)
            
            user_id = user_result_key["user_id"]
            product_names = []
            if "product_names" in user_result_key.keys():
                product_names = user_result_key["product_names"]
            photo_ids = []
            if "photo_ids" in user_result_key.keys():
                photo_ids = user_result_key["photo_ids"]
            deal_pname = get_user_product_label(product_names)
            label = "、".join(deal_pname)
            prompt += "\n请基于以上用户的各种基础信息与行为信息，推测用户可能感兴趣的10个广告产品，直接给出产品名，不要有额外描述。"
            write_json = {"user_id": user_id, "instruction": prompt, "output": label, "product_names": product_names, "photo_ids": photo_ids, "input": ""}
            result_json.append(write_json)
            max_count[len(prompt)//500] += 1
    print(max_count)
    return result_json


# 需要推理的用户文件路径
input_path = ""
create_prompt_label_data(input_path)






