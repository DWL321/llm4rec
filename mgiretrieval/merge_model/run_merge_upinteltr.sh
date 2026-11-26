ts=`date +%Y_%m_%d_%H_%M_%S`
cd /share/ad/lijupan/interest_recall/merge_multi_meta/dump_merge_model/
mkdir  -p /share/ad/lijupan/log/dump_upmodel_upinteltr
export PATH=/root/miniconda3/bin:/home/hadoop/software/hive/bin:$PATH
export HADOOP_USER_NAME=ad
export MODEL_NAME=model_upinte_ltr
export JAVA_HOME=/home/hadoop/software/java
export HADOOP_HDFS_HOME=/home/hadoop/software/hadoop
export PATH=$PATH:$HADOOP_HDFS_HOME/bin:$HADOOP_HDFS_HOME/sbin
export CLASSPATH=$($HADOOP_HDFS_HOME/bin/hadoop classpath --glob)
workspace=/data/ad
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$JAVA_HOME/jre/lib/amd64/server:/usr/local/cuda-10.0/extras/CUPTI/lib64:./:$workspace/klearn
export PYTHONPATH=$workspace/klearn:$workspace/ad-train/python/train/src/:$workspace/swan
python run_fix_matching_upinteltr.py  > /share/ad/lijupan/log/dump_upmodel_upinteltr/dump_$ts.log 2>&1
