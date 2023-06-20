mkdir -p replace_if_dummy
for i in {6..31}
do
python3 replace_if_dummy.py $i > replace_if_dummy/$i.txt
done