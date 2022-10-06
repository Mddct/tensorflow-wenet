"""Positionwise feed forward layer definition."""

import tensorflow as tf
from wenet.transformer.activations import ActivationLayer


class PositionwiseFeedForward(tf.keras.layers.Layer):
    """Positionwise feed forward layer.
    FeedForward are appied on each position of the sequence.
    The output dim is same with the input dim.
    Args:
        idim (int): Input dimenstion.
        hidden_units (int): The number of hidden units.
        dropout_rate (float): Dropout rate.
        activation (tf.keras.layers.Layer Module): Activation function
    """

    def __init__(self,
                 idim: int,
                 hidden_units: int,
                 dropout_rate: float,
                 activation: str = 'relu'):
        """Construct a PositionwiseFeedForward object."""
        super(PositionwiseFeedForward, self).__init__()

        self.out = tf.keras.Sequential([
            tf.keras.layers.Input(shape=[None, idim]),
            tf.keras.layers.Dense(hidden_units),
            ActivationLayer(name=activation),
            tf.keras.layers.Dropout(dropout_rate),
            tf.keras.layers.Dense(idim)
        ])

    def call(self, xs: tf.Tensor, training=True) -> tf.Tensor:
        """Forward function.
        Args:
            xs: input tensor (B, L, D)
        Returns:
            output tensor, (B, L, D)
        """
        return self.out(xs, training=training)
