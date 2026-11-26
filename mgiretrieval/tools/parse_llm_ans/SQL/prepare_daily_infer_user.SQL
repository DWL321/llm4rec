-- 统计日活用户
-- 快手日活用户表：kscdm.dws_ks_usr_prod_user_active_1d

drop table if exists ks_ad_dw_dev.zhangguodong_sft_conversion_base;
create table ks_ad_dw_dev.zhangguodong_sft_conversion_base as 
select 
    visitor_id as user_id
    ,product_name
    ,photo_id
from ks_ad_dw.dwd_ad_tfc_effect_llsid_did_sk_ext_di_view
where p_date between '20240825' and  '20241025'
    and source_type = 'DSP'
    and conversion_cnt>0 
    and ft_type = 'EXCYCLE'
    and action_type in ('AD_ITEM_CLICK', 'EVENT_FORM_SUBMIT', 'EVENT_CONVERSION')
group by visitor_id
,product_name
,photo_id;

drop table ks_ad_dw_dev.zhangguodong_sft_conversion_rank;
create table ks_ad_dw_dev.zhangguodong_sft_conversion_rank as 
select user_id
    ,photo_id
    ,product_name
    ,row_number() over(partition by product_name order by rand(123)) as rn
    ,row_number() over(partition by user_id order by rand(123)) as user_rn
from ks_ad_dw_dev.zhangguodong_sft_conversion_base
where photo_id>0 and user_id>0 and nvl(product_name,'')<>'';


drop table ks_ad_dw_dev.zhangguodong_sft_conversion_top;
create table ks_ad_dw_dev.zhangguodong_sft_conversion_top as 
select 
    a.user_id
    ,photo_id
    ,a.product_name
from 
ks_ad_dw_dev.zhangguodong_sft_conversion_rank a 
join 
(select product_name from ks_ad_dw_dev.zhangguodong_sft_conversion_rank group by product_name) b 
on a.product_name = b.product_name
join 
(select user_id from ks_ad_dw_dev.zhangguodong_sft_conversion_rank where user_rn>=4 group by user_id) c 
on a.user_id = c.user_id
--where rn<=20;

drop table ks_ad_dw.app_feature_ad_sft_conversion_20241015_sample;
create table ks_ad_dw.app_feature_ad_sft_conversion_20241015_sample as 
select 
user_id
,collect_list(photo_id) as photo_ids
,collect_list(product_name) as product_names
from ks_ad_dw_dev.zhangguodong_sft_conversion_top  
where abs(hash(user_id))%7 = 1
group by user_id