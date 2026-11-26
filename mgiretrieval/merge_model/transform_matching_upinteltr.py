#-*- coding : utf-8-*-
# coding:unicode_escape
import tensorflow as tf
import sys
import os

def get_map(meta, chkt, prefix=''):
    print("meta_path:" + meta)
    saver = tf.train.import_meta_graph(meta, clear_devices=True)
    graph = tf.get_default_graph()
    input_graph_def = graph.as_graph_def()
    sess = tf.Session()
    saver.restore(sess, chkt)
    mapping = {prefix + v.name: sess.run(v.name) for v in tf.trainable_variables()}
    sess.close()
    return mapping


def dump_model(input_models, input_metas, output, model_name, lod):
    all_map = {}
    cnt = 0
    for input_model,input_meta in zip(input_models.split(','), input_metas.split(',')):
        mapping = get_map(input_meta, input_model + '/model_tf', prefix='t' + str(cnt) + '/')
        all_map.update(mapping)
        tf.reset_default_graph()
        cnt += 1
    print(all_map.keys())
    meta_file = '/share/ad/lijupan/interest_recall/merge_multi_meta/' + model_name + '.meta'
    saver = tf.train.import_meta_graph(meta_file, clear_devices=True)
    graph = tf.get_default_graph()
    init_op = tf.group(tf.global_variables_initializer(), tf.local_variables_initializer(), name='init_op')
    sess = tf.Session()
    sess.run(init_op)

    def get_old_key(name, field_map):
        if name.find('embedding') >= 0 :
            field = int(name.split(':')[0].split('_')[1])
            if field in field_map:
                i, k = field_map[field]
                return 't'+ str(i) + '/embedding_' + str(k) + ':0'
        if name.find('_nn') >=0:
            return name
        return name

    field_map = {}
    for i in range(len(user_lod) - 1):
        for j in range(user_lod[i], user_lod[i+1]):
            k = j - user_lod[i]
            field_map[j] = [i, k] # ith model, kth feature
    for i in range(len(photo_lod) - 1):
        for j in range(photo_lod[i], photo_lod[i+1]):
            k = j - photo_lod[i] + user_lod[i + 1] - user_lod[i]
            field_map[j] = [i, k] # ith model, kth feature
    print(field_map)

    for v in tf.trainable_variables():
        old_key = get_old_key(v.name, field_map)
        if old_key in all_map:
            # index = [k for k in range(all_map[old_key].shape[0])]
            # v2=tf.scatter_update(v, index, all_map[old_key])
            print("old_key:{}".format(old_key))
            v2 = v.assign(all_map[old_key])
            sess.run(v2)
            print("assigned {} to {}".format(old_key, v.name))
        else:
            print("not assigned {}".format(v.name))
    #tf.train.write_graph(graph, './', "predict_model.txt", as_text=True)
    saver.save(sess, output + "/model_tf", write_meta_graph=False)
    sess.close()

    tf.reset_default_graph()
    new_mapping = get_map(meta_file, output + '/model_tf')

def parse_flags(flagfile):
  with open(flagfile) as f:
    for line in f:
      line = line.strip()
      if len(line) == 0:
        continue
      line = line.split('=')
      print(line)
      key = line[0][2:]
      value = line[1]
      os.environ[key] = value

if __name__ == "__main__":
    print("args: {}".format(sys.argv))
    input_models = sys.argv[1]
    input_meta = sys.argv[2]
    output = sys.argv[3]
    output_meta = sys.argv[4]
    config_file = sys.argv[5]
    model_name = os.environ["MODEL_NAME"]
    parse_flags(config_file)
    def get_lod(lod_str):
        return [int(x) for x in lod_str.split(',')]
    user_lod = get_lod(os.environ["user_lod"])
    photo_lod = get_lod(os.environ["photo_lod"])
    dense_user_lod = get_lod(os.environ["dense_user_lod"])
    dense_photo_lod = get_lod(os.environ["dense_photo_lod"])

    lod = (user_lod, photo_lod, dense_user_lod, dense_photo_lod)

    dump_model(input_models, input_meta, output, model_name, lod)
