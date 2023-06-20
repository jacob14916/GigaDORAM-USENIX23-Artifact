mkdir -p dummy_check
for i in {6..31}
do
python3 dummy_check.py $i > dummy_check/$i.txt
done