import subprocess
import os

def run_cmd(cmd):
    print('\nrun cmd: ' + cmd + '\n')
    # cmd_out = subprocess.check_output(cmd, shell=True, cwd='.', stderr=subprocess.STDOUT)
    cmd_out = subprocess.check_output(cmd, shell=True, cwd='.',)
    cmd_out = str(cmd_out, encoding = "utf8")
    print(cmd_out)
    return cmd_out

def run_cmds(cmds):
    #cmd = ' && '.join(cmds)
    for cmd in cmds:
        run_cmd(cmd)

def get_last_model(model_path):
    cmd="hdfs dfs -cat  " + model_path + "/model_version | sed 1d | tail -n 1 | awk -F'\\t' '{print $1}' | awk -F'/' '{print $NF}'"
    return run_cmd(cmd)

def hdfs_download(src, dst):
    cmd = 'hadoop fs -get ' + src + ' ' + dst
    run_cmd(cmd)

def mkdir(path):
    cmd = 'mkdir -p ' + path
    run_cmd(cmd)
    
def rm(path):
    cmd = 'rm -rf ' + path
    run_cmd(cmd)

def main():
    model_path1 = '/home/ad/model/dsp_recall_hyp_gjw_lst_hyp3/'
    model1 = get_last_model(model_path1).strip()
    print("model1:{}".format(model1))
    print(model1.strip().split('.')[1])
    model1_version = int(model1.strip().split('.')[1])

    
    model_path2 = '/home/ad/model_offline/dsp_recall_ljp_matememb20/'
    model2 = get_last_model(model_path2).strip()
    model2_version = int(model2.strip().split('.')[1])

    model_name = os.getenv('MODEL_NAME')

    model_path = "/home/ad/model/" + model_name + ''
    out_model = get_last_model(model_path)

    if not model1 or not model2:
       return 

    if out_model:
        out_version = int(out_model.strip().split('.')[1])
        if out_version >= model1_version and out_version >= model2_version:
            return
    out_version = max(model2_version, model1_version)
    out_path = 'output_multi_upinteltr/model_tf.' + str(out_version)

    #'''
    cmds = []
    cmds.append('rm -rf ' + out_path)
    cmds.append('mkdir -p ' + out_path)
    run_cmds(cmds)

    mkdir('./upinteltr_input1/')
    rm('./upinteltr_input1/*')
    mkdir('./upinteltr_input2/')
    rm('./upinteltr_input2/*')
    hdfs_download(model_path1 + model1, './upinteltr_input1/')
    hdfs_download(model_path2 + model2, './upinteltr_input2/')

    multi_emb_input1 = './upinteltr_input1/' + model1
    multi_emb_input2 = './upinteltr_input2/' + model2
    input_path = ','.join([multi_emb_input1,multi_emb_input2])
    input_meta='/share/ad/lijupan/interest_recall/merge_multi_meta/model_infer_new_curveltr.meta,/share/ad/lijupan/interest_recall/merge_multi_meta/matememb20.meta'
    out_meta='model_upinte_ltr.meta'
    cmd = 'python transform_matching_upinteltr.py ' + ' '.join([input_path, input_meta, out_path, out_meta, 'dsp_recall_ljp_upinte_ltr.config'])
    run_cmd(cmd)
    #'''
    cmds = []
    with open('output_multi_upinteltr/model_version', 'w+') as f:
        f.write('\n{}//model_tf.{}\tmd5\t1'.format(model_path, out_version))
    cmds.append('hadoop fs -mkdir -p {} '.format(model_path))
    cmds.append('hadoop fs -put -f {} {}'.format(out_path, model_path))
    cmds.append('hadoop fs -put -f output_multi_upinteltr/model_version {}'.format(model_path))
    cmds.append('hadoop fs -ls {}'.format(model_path))
    cmds.append('rm -rf {}'.format(out_path))
    run_cmds(cmds)
    
if __name__ == '__main__':
    main()
