# -*- coding: utf-8 -*-
from framework_wrappers import BtqClient
from framework_wrappers import BtqException
from framework_wrappers import BtqErrorCode

topic = "dsp_lps_ljp_casQv1"
reader = "test"

while True:
    try:
        m = BtqClient.consume(topic, reader)
        print(m.message, m.timestamp)
    except BtqException as e:
        if e.code == BtqErrorCode.NO_MORE_DATA:
            break
        raise e from BtqException
