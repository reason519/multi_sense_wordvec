"""
过滤掉低频词语，生成40w的高频单词和对应的语义项。
例如：
python process_voc_40w.py context_voc_all.txt sense_voc_all.txt ord_to_sense_all.txt context_voc.txt sense_voc.txt word_to_sense.txt
输入：
    包含所有词语词典：context_voc_all.txt
    包含所有语义项词典：sense_voc_all.txt
    包含所有词语与语义项之间的对应关系：word_to_sense_all.txt
输出：
    40w词语词典：context_voc_all.txt
    40w语义项词典：sense_voc_all.txt
    40w词语与语义项之间的对应关系：word_to_sense_all.txt
"""
import sys
import argparse

def parse_arguments(argv):
    parser=argparse.ArgumentParser()
    parser.add_argument('context_path_all', type=str, help='all word vocabulary ')
    parser.add_argument('sense_path_all', type=str, help='all sense vocabulary ')
    parser.add_argument('word_to_sense_path_all', type=str, help='Correspondence between words and sense ')

    parser.add_argument('context_path', type=str, help='word vocabulary ')
    parser.add_argument('sense_path', type=str, help='sense vocabulary ')
    parser.add_argument('word_to_sense_path', type=str, help='Correspondence between words and sense ')
    return parser.parse_args(argv)
def main(args):
    context_voc={}
    with open(args.context_path_all,'r',encoding='utf-8') as f:
        for line in f:
            word=line.split(' ')
            context_voc[word[0].strip()]=int(word[1])

    #context_voc=sorted(context_voc.items(), key=lambda d:d[1],reverse = True)

    sense_voc={}
    with open(args.sense_path_all,'r',encoding='utf-8') as f:
        for line in f:
            sense=line.split(' ')
            sense_voc[sense[0].strip()]=int(sense[1])

    word_to_senses={}
    with open(args.word_to_sense_path_all,'r',encoding='utf-8') as f:
        for line in f:
            word_sen=line.split(' ')
            word_to_senses[word_sen[0].strip()]=[x.strip() for x in word_sen[1:] if x!='']


    context_voc=sorted(context_voc.items(), key=lambda d:d[1],reverse = True)
    with open(args.context_path,'w',encoding='utf-8') as f:
        for d in context_voc[:400000]:
            f.write(d[0].strip()+' '+str(d[1]).strip())
            f.write('\n')
    print(400000)

    _sense_voc={}
    for d in context_voc[:400000]:
        for sense in word_to_senses[d[0]]:
            if sense not in _sense_voc:
                _sense_voc[sense]=sense_voc[sense]
    with open(args.sense_path,'w',encoding='utf-8') as f:
        for d in _sense_voc:
            f.write(d.strip()+' '+str(_sense_voc[d]).strip())
            f.write('\n')
    print(len(_sense_voc))

    _word_to_sense={}
    for d in context_voc[:400000]:
        if d[0] in word_to_senses:
            _word_to_sense[d[0]]=word_to_senses[d[0]]
    print('word_to_sense len :',len(_word_to_sense))
    with open(args.word_to_sense_path,'w',encoding='utf-8') as f:
        for d in _word_to_sense:

            f.write(d.strip())
            for dd in _word_to_sense[d]:
                f.write(' '+dd.strip())
            f.write('\n')
if __name__=='__main__':
    main(parse_arguments(sys.argv[1:]))
# word_to_senses={}
# with open('word_to_sense_all.txt','r',encoding='utf-8') as f:
#     for line in f:
#         word_sen=line.split(' ')
#         word_to_senses[word_sen[0]]=word_sen[1:]
#
# sense_to_word={}
# for d in word_to_senses:
#     for dd in word_to_senses[dd]:
#         sense_to_word[dd]=d
#
# with open('sense_to_word_all.txt','w',encoding='utf-8') as f:
#     for d in sense_to_word:
#         f.write(d[0]+' '+str(d[1]))
#         f.write('\n')

# sense_to_word={}
# with open('sense_to_word_all.txt','r',encoding='utf-8') as f:
#     for line in f:
#         word_sen = line.split(' ')
#         sense_to_word[word_sen[0]]=word_sen[1].strip()
#
# sense_voc=sorted(sense_voc.items(), key=lambda d:d[1],reverse = True)
# print(len(sense_voc))
# print(sense_voc[:10])
# with open('sense_voc.txt','w',encoding='utf-8') as f:
#     for d in sense_voc[:100000]:
#         f.write(d[0].strip()+' '+str(d[1]).strip())
#         f.write('\n')
#
# _cont_voc={}
# for d in sense_voc[:100000]:
#     if d[0] not in _cont_voc:
#         if d[0] not in sense_to_word:
#           #  print(d[0])
#             _cont_voc[d[0]] = int(context_voc[d[0]])
#         else:
#             _cont_voc[sense_to_word[d[0]]]=int(context_voc[sense_to_word[d[0]]])
#   #  else :
#
# print("context:",len(_cont_voc))
# with open('context_voc.txt','w',encoding='utf-8') as f:
#     for d in _cont_voc:
#         f.write(d.strip()+' '+str(_cont_voc[d]).strip())
#         f.write('\n')
#
# _word_to_sense={}
# for d in sense_voc[:100000]:
#     if d[0] in sense_to_word:
#         w_s=sense_to_word[d[0]]
#     else:
#         continue
#     if w_s in _word_to_sense:
#         if w_s not in _word_to_sense[w_s]:
#             _word_to_sense[w_s].add(d[0])
#     else:
#         _word_to_sense[w_s] = set()
#         _word_to_sense[w_s].add(d[0])
#
# print('word_to_sense len :',len(_word_to_sense))
# with open('word_to_sense.txt','w',encoding='utf-8') as f:
#     for d in _word_to_sense:
#
#         f.write(d.strip())
#         for dd in _word_to_sense[d]:
#             f.write(' '+dd.strip())
#         f.write('\n')

