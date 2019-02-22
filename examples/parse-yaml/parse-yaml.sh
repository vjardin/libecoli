#!/bin/sh

set -e

# use a safer version of echo (no option)
echo()
{
	printf "%s\n" "$*"
}

debug()
{
	echo "$@" >&2
}

# $1: node sequence number (ex: ec_node4)
ec_parse_get_first_child()
{
	local first_child=${1}_first_child
	echo $(eval 'echo ${'$first_child'}')
}

# $1: node sequence number (ex: ec_node4)
ec_parse_get_next()
{
	local next=${1}_next
	echo $(eval 'echo ${'$next'}')
}

# $1: node sequence number (ex: ec_node4)
ec_parse_iter_next()
{
	local seq=${1#ec_node}
	seq=$((seq+1))
	local next=ec_node${seq}
	if [ "$(ec_parse_get_id $next)" != "" ]; then
		echo $next
	fi
}

# $1: node sequence number (ex: ec_node4)
ec_parse_get_id()
{
	local id=${1}_id
	echo $(eval 'echo ${'$id'}')
}

# $1: node sequence number (ex: ec_node4)
ec_parse_get_strvec_len()
{
	local strvec_len=${1}_strvec_len
	echo $(eval 'echo ${'$strvec_len'}')
}

# $1: node sequence number (ex: ec_node4)
# $2: index in strvec
ec_parse_get_str()
{
	if [ $# -ne 2 ]; then
		return
	fi
	local str=${1}_str${2}
	echo $(eval 'echo ${'$str'}')
}

# $1: node sequence number (ex: ec_node4)
# $2: node id (string)
ec_parse_find_first()
{
	if [ $# -ne 2 ]; then
		return
	fi
	local node_seq=$1
	while [ "$node_seq" != "" ]; do
		local id=$(ec_parse_get_id $node_seq)
		if [ "$id" = "$2" ]; then
			echo $node_seq
			return 0
		fi
		node_seq=$(ec_parse_iter_next $node_seq)
	done
}

path=$(dirname $0)

yaml=$(mktemp)
cat << EOF > $yaml
type: or
children:
- type: seq
  id: hello
  help: Say hello to someone
  children:
  - type: str
    string: hello
  - type: or
    id: name
    help: Name of the person to greet
    children:
    - type: str
      string: john
    - type: str
      string: mike
- type: seq
  id: goodbye
  help: Say good bye to someone
  children:
  - type: str
    string: good
  - type: str
    string: bye
  - type: or
    id: name
    help: Name of the person to greet
    children:
    - type: str
      string: mary
    - type: str
      string: jessica
EOF

output=$(mktemp)
match=1
$path/build/parse-yaml -i $yaml -o $output || match=0
if [ "$match" = "1" ]; then
	cat $output
	. $output
	name=$(ec_parse_get_str $(ec_parse_find_first ec_node1 name) 0)
	hello=$(ec_parse_get_str $(ec_parse_find_first ec_node1 hello) 0)

	if [ "$hello" != "" ]; then
		echo "$name says hello to you!"
	else
		echo "$name says good bye to you!"
	fi
else
	echo "no match"
fi
rm $output
rm $yaml
