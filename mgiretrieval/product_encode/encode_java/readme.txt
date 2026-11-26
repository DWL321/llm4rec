# 编译
javac ProductHashCode.java
# 运行
java -cp . ProductHashCode "Sample Product Name"

# 在 fileB 中查找在 fileA 中出现的行
grep -F -f aa java_hash_unique_product_ids.txt  > bb

# 将本地文件载入 hive 表中的方法
export hive_table=ad_feature_production_dev.java_hash_product_name_and_id
export local_file_path=dataset/local_java_hash_product_id_sep_tab
hive -e "LOAD DATA LOCAL INPATH '$local_file_path' INTO TABLE $hive_table PARTITION (p_date='${yesterday}')"

# 处理 1027 的全量 product_name
model_strategy_e.outerloop_all_product_name


# 创建天级别增量表
CREATE TABLE IF NOT EXISTS model_strategy_e.java_hash_product_name_and_id_inc_daily (
  `product_name` string COMMENT '产品名',
  `product_id` bigint COMMENT '产品id'
) COMMENT 'java hash product name and id'
partitioned by(p_date string)
ROW FORMAT DELIMITED
FIELDS TERMINATED BY '\t'
LINES TERMINATED BY '\n'
STORED AS TEXTFILE;

# 创建天级别全量表

insert overwrite table model_strategy_e.java_hash_product_name_and_id_all_daily partition(p_date = '{{ ds_nodash }}')

select
    product_name, 
    product_id
from model_strategy_e.java_hash_product_name_and_id_all_daily
where p_date = "{{ds_nodash - 1}}"
union
select
    product_name, 
    product_id
from model_strategy_e.java_hash_product_name_and_id_inc_daily
where p_date = "{{ds_nodash}}"

# 天级别将 embedding 写入 redis 的任务
https://tg.corp.kuaishou.com/develop/#/task/dag-detail?groupId=2550&clusterId=airflow_idp1&dagId=38787200&businessType=hiveQuery&groupName=%E7%A4%BE%E7%A7%91%E5%95%86%E4%B8%9A%E5%8C%96%E6%A8%A1%E5%9E%8B%E5%8F%8A%E7%A1%AC%E5%B9%BF%E7%AD%96%E7%95%A5%E4%B8%93%E7%94%A8E%E7%BB%84

# 基于新版实时 trigger 的部署
https://kaiserving.corp.kuaishou.com/deployment-model/own-model/information?modelId=62769&service=185654&tabkey=null
