#!/bin/bash
status=$(ps|grep -c /usr/share/openclash/yml_groups_get.sh)
[ "$status" -gt "3" ] && exit 0

START_LOG="/tmp/openclash_start.log"
CFG_FILE="/etc/config/openclash"
servers_update=$(uci get openclash.config.servers_update 2>/dev/null)
servers_if_update=$(uci get openclash.config.servers_if_update 2>/dev/null)
rulesource=$(uci get openclash.config.rule_source 2>/dev/null)
CONFIG_FILE=$(uci get openclash.config.config_path 2>/dev/null)
CONFIG_NAME=$(echo "$CONFIG_FILE" |awk -F '/' '{print $5}' 2>/dev/null)
UPDATE_CONFIG_FILE=$(uci get openclash.config.config_update_path 2>/dev/null)
UPDATE_CONFIG_NAME=$(echo "$UPDATE_CONFIG_FILE" |awk -F '/' '{print $5}' 2>/dev/null)

if [ ! -z "$UPDATE_CONFIG_FILE" ]; then
   CONFIG_FILE="$UPDATE_CONFIG_FILE"
   CONFIG_NAME="$UPDATE_CONFIG_NAME"
fi

if [ -z "$CONFIG_FILE" ]; then
	CONFIG_FILE="/etc/openclash/config/$(ls -lt /etc/openclash/config/ | grep -E '.yaml|.yml' | head -n 1 |awk '{print $9}')"
	CONFIG_NAME=$(echo "$CONFIG_FILE" |awk -F '/' '{print $5}' 2>/dev/null)
fi

if [ -z "$CONFIG_NAME" ]; then
   CONFIG_FILE="/etc/openclash/config/config.yaml"
   CONFIG_NAME="config.yaml"
fi

BACKUP_FILE="/etc/openclash/backup/$(echo "$CONFIG_FILE" |awk -F '/' '{print $5}' 2>/dev/null)"

if [ ! -s "$CONFIG_FILE" ] && [ ! -s "$BACKUP_FILE" ]; then
   exit 0
elif [ ! -s "$CONFIG_FILE" ] && [ -s "$BACKUP_FILE" ]; then
   mv "$BACKUP_FILE" "$CONFIG_FILE"
fi

echo "开始更新【$CONFIG_NAME】的策略组配置..." >$START_LOG

/usr/share/openclash/yml_groups_name_get.sh
[ ! -z "$(grep "读取错误" /tmp/Proxy_Group)"] && {
	echo "读取错误，配置文件【$CONFIG_NAME】异常！" >$START_LOG
	uci commit openclash
	sleep 5
	echo "" >$START_LOG
	exit 0
}

cp /tmp/Proxy_Group /tmp/yaml_group.yaml
sed -i '/DIRECT/d' /tmp/yaml_group.yaml 2>/dev/null
sed -i '/REJECT/d' /tmp/yaml_group.yaml 2>/dev/null

if [ "$servers_update" -ne "1" ] || [ "$servers_if_update" != "1" ] || [ -z "$(grep "config groups" "$CFG_FILE")" ]; then
echo "正在删除旧配置..." >$START_LOG
#删除策略组
   group_num=$(grep "config groups" "$CFG_FILE" |wc -l)
   for ((i=$group_num;i>=0;i--))
	 do
	    if [ "$(uci get openclash.@groups["$i"].config)" = "$CONFIG_NAME" ] || [ "$(uci get openclash.@groups["$i"].config)" = "all" ]; then
	       uci delete openclash.@groups["$i"] 2>/dev/null
	       uci commit openclash
	    fi
	 done
#删除启用节点
   server_num=$(grep "config servers" "$CFG_FILE" |wc -l)
   for ((i=$server_num;i>=0;i--))
	 do
	    if [ "$(uci get openclash.@servers["$i"].config)" = "$CONFIG_NAME" ] || [ "$(uci get openclash.@servers["$i"].config)" = "all" ]; then
	    	 if [ "$(uci get openclash.@servers["$i"].enabled)" = "1" ]; then
	          uci delete openclash.@servers["$i"] 2>/dev/null
	          uci commit openclash
	       fi
	    fi
	 done
#删除启用代理集
   provider_num=$(grep "config proxy-provider" "$CFG_FILE" |wc -l)
   for ((i=$provider_num;i>=0;i--))
	 do
	    if [ "$(uci get openclash.@proxy-provider["$i"].config)" = "$CONFIG_NAME" ] || [ "$(uci get openclash.@proxy-provider["$i"].config)" = "all" ]; then
	       if [ "$(uci get openclash.@proxy-provider["$i"].enabled)" = "1" ]; then
	          uci delete openclash.@proxy-provider["$i"] 2>/dev/null
	          uci commit openclash
	       fi
	    fi
	 done
else
   /usr/share/openclash/yml_proxys_get.sh
   exit 0
fi

count=1
file_count=1
match_group_file="/tmp/Proxy_Group"
group_file="/tmp/yaml_group.yaml"
line=$(sed -n '/name:/=' $group_file)
num=$(grep -c "name:" $group_file)
   
cfg_get()
{
	echo "$(grep "$1" "$2" 2>/dev/null |awk -v tag=$1 'BEGIN{FS=tag} {print $2}' 2>/dev/null |sed 's/,.*//' 2>/dev/null |sed 's/\}.*//' 2>/dev/null |sed 's/^ \{0,\}//g' 2>/dev/null |sed 's/ \{0,\}$//g' 2>/dev/null)"
}

for n in $line
do
   single_group="/tmp/group_$file_count.yaml"
   
   [ "$count" -eq 1 ] && {
      startLine="$n"
  }

   count=$(expr "$count" + 1)
   if [ "$count" -gt "$num" ]; then
      endLine=$(sed -n '$=' $group_file)
   else
      endLine=$(expr $(echo "$line" | sed -n "${count}p") - 1)
   fi
  
   sed -n "${startLine},${endLine}p" $group_file >$single_group
   startLine=$(expr "$endLine" + 1)
   
   #type
   group_type="$(cfg_get "type:" "$single_group")"
   #name
   group_name="$(cfg_get "name:" "$single_group")"
   #test_url
   group_test_url="$(cfg_get "url:" "$single_group")"
   #test_interval
   group_test_interval="$(cfg_get "interval:" "$single_group")"

   echo "正在读取【$CONFIG_NAME】-【$group_type】-【$group_name】策略组配置..." >$START_LOG
   
   name=openclash
   uci_name_tmp=$(uci add $name groups)
   uci_set="uci -q set $name.$uci_name_tmp."
   uci_add="uci -q add_list $name.$uci_name_tmp."
   if [ "$servers_update" -eq "1" ]; then
      ${uci_set}config="all"
   else
      ${uci_set}config="$CONFIG_NAME"
   fi
   ${uci_set}name="$group_name"
   ${uci_set}old_name="$group_name"
   ${uci_set}old_name_cfg="$group_name"
   ${uci_set}type="$group_type"
   ${uci_set}test_url="$group_test_url"
   ${uci_set}test_interval="$group_test_interval"
   
   #other_group
   cat $single_group |while read line
   do 
      if [ -z "$(echo "$line" |grep "^ \{0,\}-")" ]; then
        continue
      fi
      
      group_name1=$(echo "$line" |grep -v "name:" 2>/dev/null |grep "^ \{0,\}-" 2>/dev/null |awk -F '^ \{0,\}-' '{print $2}' 2>/dev/null |sed 's/^ \{0,\}//' 2>/dev/null |sed 's/ \{0,\}$//' 2>/dev/null)
      group_name2=$(echo "$line" |awk -F 'proxies: \\[' '{print $2}' 2>/dev/null |sed 's/].*//' 2>/dev/null |sed 's/^ \{0,\}//' 2>/dev/null |sed 's/ \{0,\}$//' 2>/dev/null |sed 's/ \{0,\}, \{0,\}/#,#/g' 2>/dev/null)
      proxies_len=$(sed -n '/proxies:/=' $single_group 2>/dev/null)
      use_len=$(sed -n '/use:/=' $single_group 2>/dev/null)
      name1_len=$(sed -n "/${group_name1}/=" $single_group 2>/dev/null)
      name2_len=$(sed -n "/${group_name2}/=" $single_group 2>/dev/null)

      if [ -z "$group_name1" ] && [ -z "$group_name2" ]; then
         continue
      fi
      
      if [ ! -z "$group_name1" ] && [ -z "$group_name2" ]; then
         if [ "$proxies_len" -le "$use_len" ]; then
            if [ "$name1_len" -le "$use_len" ] && [ ! -z "$(grep -F "$group_name1" $match_group_file)" ] && [ "$group_name1" != "$group_name" ]; then
               ${uci_add}other_group="$group_name1"
            fi
         else
            if [ "$name1_len" -ge "$proxies_len" ] && [ ! -z "$(grep -F "$group_name1" $match_group_file)" ] && [ "$group_name1" != "$group_name" ]; then
               ${uci_add}other_group="$group_name1"
            fi
         fi
      elif [ -z "$group_name1" ] && [ ! -z "$group_name2" ]; then
         group_num=$(expr $(echo "$group_name2" |grep -c "#,#") + 1)
         if [ "$group_num" -le 1 ]; then
            if [ ! -z "$(grep -F "$group_name2" $match_group_file)" ] && [ "$group_name2" != "$group_name" ]; then
               ${uci_add}other_group="$group_name2"
            fi
         else
            group_nums=1
            while [[ "$group_nums" -le "$group_num" ]]
            do
               other_group_name=$(echo "$group_name2" |awk -v t="${group_nums}" -F '#,#' '{print $t}' 2>/dev/null)
               if [ ! -z "$(grep -F "$other_group_name" $match_group_file 2>/dev/null)" ] && [ "$other_group_name" != "$group_name" ]; then
                  ${uci_add}other_group="$other_group_name"
               fi
               group_nums=$(expr "$group_nums" + 1)
            done
         fi
      fi
      
   done
   
   file_count=$(expr "$file_count" + 1)
    
done

uci commit openclash
/usr/share/openclash/yml_proxys_get.sh