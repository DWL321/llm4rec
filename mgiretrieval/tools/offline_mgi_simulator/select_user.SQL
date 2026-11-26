create table if not exists model_strategy_e.long_act_user_for_test (
    visitor_id bigint
)
stored as textfile
TBLPROPERTIES (
  'kuaishou.dp-mdc.lifecycle.day' = '7'
);

insert
    overwrite table model_strategy_e.long_act_user_for_test
select
    visitor_id
from model_strategy_e.upmodel_user_seq_for_inf_fix
where p_date ='{{ ds_nodash}}'
    and length(pdct_seq) > 2
limit 100000