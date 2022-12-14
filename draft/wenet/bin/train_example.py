import tensorflow as tf
import yaml
from numpy import dtype
from wenet.dataset.dataset import Dataset
from wenet.utils.init_model import init_model

config = "./conformer.yaml"
with open(config, 'r') as fin:
    configs = yaml.load(fin, Loader=yaml.FullLoader)

input_dim = configs['dataset_conf']['fbank_conf']['num_mel_bins']

configs['input_dim'] = input_dim
configs['output_dim'] = 5000
configs['cmvn_file'] = None
configs['is_json_cmvn'] = True

model = init_model(configs)

input_dim = configs['dataset_conf']['fbank_conf']['num_mel_bins']

configs['input_dim'] = input_dim
configs['output_dim'] = 5000
configs['cmvn_file'] = None
configs['is_json_cmvn'] = True

# just for simple test, future to tf.text
symbol_table = read_symbol_table(
    '/Users/mddct/work/tensorflow-wenet/draft/wenet/dataset/units.txt')
dataset = Dataset(configs['dataset_conf'],
                  "../dataset/train.txt",
                  symbol_table,
                  data_type='raw',
                  global_batch_size=2)

# for batch in dataset:
#     feats, feats_length, text, text_length = batch
#     text_length = tf.cast(text_length, dtype=tf.int32)
#     text = tf.cast(text, dtype=tf.int32)
#     loss = model(feats, feats_length, text, text_length)
#     print(loss)
# print(feats_length.dtype, text.dtype, text_length.dtype)
dummy_speech = tf.ones([2, 100, 80], dtype=tf.float32)
speech_length = tf.constant([100, 100], dtype=tf.int32)
dummy_text = tf.constant([[1, 2], [1, 2]], dtype=tf.int32)
text_length = tf.constant([2, 2], dtype=tf.int32)

loss = model(dummy_speech,
             speech_length,
             dummy_text,
             text_length,
             training=True)
print(loss['loss_ctc'])

# # print(model.summary())
# # print(model.trainable_variables)
# # print(model.non_trainable_variables)

# checkpoint = tf.train.Checkpoint(model)
# # checkpoint.save("model/u2++")
# # reader = tf.train.load_checkpoint("model/")

# # restore partial variable
# ## https://www.tensorflow.org/guide/checkpoint#manual_checkpointing
# ## https://github.com/tensorflow/tensorflow/issues/37793

# checkpoint.restore("model/u2++-1").expect_partial()
