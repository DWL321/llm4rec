-- ad_asset.app_pub_feature_user_map_llm_interpret_1d

-- CREATE TABLE IF NOT EXISTS ad_asset.app_pub_feature_user_map_llm_interpret_1d 
--(
-- user_id bigint comment '用户id',
-- photo_consume_info array<map<string,string>> comment '视频消费信息',
-- live_consume_info array<map<string,string>> comment '直播消费信息',
-- follow_info array<map<string,string>> comment '关注信息',
-- im_msg_info array<map<string,string>> comment '私信信息',
-- photo_comment_content_info array<map<string,string>> comment '视频评论信息',
-- live_comment_content_info array<map<string,string>> comment '直播评论信息',
-- trade_order_info array<map<string,string>> comment '电商交易信息',
-- local_life_info array<map<string,string>> comment '本地生活信息',
-- keyword_info array<map<string,string>> comment '搜索关键词信息',
-- profile_info array<map<string,string>> comment '用户画像信息',
-- consuming_ability_info array<map<string,string>> comment '消费能力信息',
-- install_applist_info array<map<string,string>> comment '安装applist信息',
-- active_applist_info array<map<string,string>> comment '活跃applist信息',
-- update_photo_info array<map<string,string>> comment '视频更新信息',
-- poi_info array<map<string,string>> comment 'poi信息',
-- contact_name_rev_info  array<map<string,array<string>>> comment '通信录信息',
-- car_4s_info array<map<string,string>> comment '4s车店访问信息',
-- ad_info array<map<string,string>> comment '广告信息',
---- photo_hetu_consume_info array<map<string,string>> comment '视频河图访问信息',
-- live_hetu_consume_info array<map<string,string>> comment '直播河图信息',
-- tag_info array<map<string,string>> comment '用户标签信息',
-- trade_cart_info array<map<string,string>> comment '电商加购信息',
-- photo_hetu_consume_info array<map<string,string>> comment '河图视频消费行为'
--)
-- COMMENT '用户大模型输入信息'
-- PARTITIONED BY (p_date string COMMENT '日期，eg：20230101')



set hive.unarchive.automatic.enable=true;
set  spark.executor.memory=20g; 
set  spark.executor.memoryOverhead=4096;
set spark.driver.memory=15g;
set spark.driver.memory.fraction=0.1;
set spark.driver.memoryOverhead=4096;
--union并行参数
set hive.exec.parallel = true;
set hive.exec.parallel.thread.number=12; 
set spark.sql.files.maxPartitionBytes=134217728;
set spark.sql.adaptive.shuffle.targetPostShuffleInputSize = 134217728;






insert overwrite table ad_asset.app_pub_feature_user_map_llm_interpret_1d partition (p_date='{{ ds_nodash }}')
select 
    a.user_id
    ,b.photo_consume_info as photo_consume_info
    ,c.live_consume_info as live_consume_info
    ,d.follow_info as follow_info
    ,e.im_msg_info as im_msg_info
    ,f.photo_comment_content_info as photo_comment_content_info
    ,g.live_comment_content_info as live_comment_content_info
    ,h.trade_order_info as trade_order_info
    ,i.local_life_info as local_life_info
    ,j.keyword_info as keyword_info
    ,k.profile_info as profile_info
    ,l.consuming_ability_info as consuming_ability_info
    ,m.install_applist_info as install_applist_info
    ,n.active_applist_info as active_applist_info
    ,o.update_photo_info as update_photo_info
    ,p.poi_info as poi_info
    ,q.contact_name_rev_info as contact_name_rev_info
    ,r.car_4s_info as car_4s_info
    ,s.ad_info as ad_info
    ,u.live_hetu_consume_info
    ,v.tag_info
    ,w.trade_cart_info
    ,t.photo_hetu_consume_info as photo_hetu_consume_info
from  
    ad_asset.dmp_mmu_user_feature_ad_base a 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_clue_photo_consume_info b 
    on a.user_id = b.user_id 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_clue_live_consume_info c
    on a.user_id = c.user_id 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_clue_follow_info d 
    on a.user_id = d.user_id 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_clue_im_msg_info e
    on a.user_id = e.user_id 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_clue_photo_comment_content_info f
    on a.user_id = f.user_id 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_clue_live_comment_content_info g
    on a.user_id = g.user_id 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_clue_trade_order_info h
    on a.user_id = h.user_id 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_clue_local_life_info i
    on a.user_id = i.user_id 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_clue_keyword_info j
    on a.user_id = j.user_id 
left outer join 
    ad_asset.dmp_mmu_user_feature_ad_profile_info k
    on a.user_id = k.user_id 
left outer join 
ad_asset.dmp_mmu_user_feature_ad_consuming_ability_info l
on a.user_id = l.user_id 
left outer join 
ad_asset.dmp_mmu_user_feature_ad_install_applist_info m 
on a.user_id = m.user_id 
left outer join 
ad_asset.dmp_mmu_user_feature_ad_active_applist_info n 
on a.user_id = n.user_id 
left outer join 
ad_asset.dmp_mmu_user_feature_ad_update_photo_info o
on a.user_id = o.user_id 
left outer join 
ad_asset.dmp_mmu_user_feature_ad_poi_info p
on a.user_id = p.user_id 
left outer join 
ad_asset.dmp_mmu_user_feature_ad_contact_name_rev_info q
on a.user_id = q.user_id 
left outer join 
ad_asset.dmp_mmu_user_feature_ad_car_4s_info r
on a.user_id = r.user_id 
left outer join 
ad_asset.dmp_mmu_user_feature_ad_clue_ad_info s
on a.user_id = s.user_id
left outer join 
ad_asset.dmp_mmu_user_feature_ad_clue_photo_hetu_consume_info t 
on a.user_id = t.user_id
left outer join 
ad_asset.dmp_mmu_user_feature_ad_clue_live_hetu_consume_info u
on a.user_id = u.user_id
left outer join 
ad_asset.dmp_mmu_user_feature_ad_clue_tag_info v 
on a.user_id = v.user_id
left outer join 
ad_asset.dmp_mmu_user_feature_ad_clue_trade_cart_info w
on a.user_id = w.user_id
;