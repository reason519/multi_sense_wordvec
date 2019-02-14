"""
根据语义标注文本文件用于用来生成词语词典、语义项词典和词语与语义项词典的对应关系。
例如：
python gen_voc.py F://paper//data//wiki2010/wiki_pwd.txt context_voc_all.txt  sense_voc_all.txt  word_to_sense_all.txt
输入：区分不同语义的标注文件 F://paper//data//wiki2010/wiki_pwd.txt
输出：
    词语词典：context_voc_all.txt
    语义项词典：sense_voc_all.txt
    词语与语义项之间的对应关系：word_to_sense_all.txt
"""
import sys
import argparse

def parse_arguments(argv):
    parser=argparse.ArgumentParser()
    parser.add_argument('pwd_file_path',type=str,help='Input the tagged wsd file ')
    parser.add_argument('context_path', type=str, help='word vocabulary ')
    parser.add_argument('sense_path', type=str, help='sense vocabulary ')
    parser.add_argument('word_to_sense_path', type=str, help='Correspondence between words and sense ')
    return parser.parse_args(argv)

def main(args):
    wiki_path=args.pwd_file_path

    word_to_senses={}
    sense_voc={}
    context_voc={}
    with open(wiki_path,'r',encoding='utf-8') as f:
        for line in f:
            sents=[x.strip() for x in line.split(' ')]
            for ws in sents:
                w_s=ws.split('|')
                if len(w_s)>1:
                    if w_s[0] in context_voc:
                        context_voc[w_s[0]]+=1
                    else:
                        context_voc[w_s[0]]=0

                    if w_s[1] in sense_voc:
                        sense_voc[w_s[1]]+=1
                    else:
                        sense_voc[w_s[1]]=0

                    if w_s[0] in word_to_senses:
                        if w_s[1] not in word_to_senses[w_s[0]]:
                            word_to_senses[w_s[0]].add(w_s[1])
                    else:
                        word_to_senses[w_s[0]]=set()
                        word_to_senses[w_s[0]].add(w_s[1])

                else:
                    if w_s[0] in context_voc:
                        context_voc[w_s[0]]+=1
                    else:
                        context_voc[w_s[0]]=0

                    if w_s[0] in sense_voc:
                        sense_voc[w_s[0]]+=1
                    else:
                        sense_voc[w_s[0]]=0

                    if w_s[0] not in word_to_senses:
                        word_to_senses[w_s[0]] = set()
                        word_to_senses[w_s[0]].add(w_s[0])

    print(len(context_voc),len(sense_voc),len(word_to_senses))
    context_voc=sorted(context_voc.items(), key=lambda d:d[1],reverse = True)
    with open(args.context_path,'w',encoding='utf-8') as f:
        for d in context_voc:
            if d[0].strip()!='':
                f.write(d[0]+' '+str(d[1]))
                f.write('\n')

    sense_voc=sorted(sense_voc.items(), key=lambda d:d[1],reverse = True)
    with open(args.sense_path,'w',encoding='utf-8') as f:
        for d in sense_voc:
            if d[0].strip() != '':
                f.write(d[0]+' '+str(d[1]))
                f.write('\n')

    with open(args.word_to_sense_path,'w',encoding='utf-8') as f:
        for d in word_to_senses:
            if d.strip() != '':
                f.write(d)
                for dd in word_to_senses[d]:
                    f.write(' '+dd)
                f.write('\n')

if __name__=='__main__':
    main(parse_arguments(sys.argv[1:]))
# sense_to_word={}
# for d in word_to_senses:
#     for dd in word_to_senses[dd]:
#         sense_to_word[dd]=d
#
# with open('sense_to_word_all.txt','w',encoding='utf-8') as f:
#     for d in sense_to_word:
#         if d.strip() != '':
#             f.write(d+' '+str(sense_to_word[d]))
#             f.write('\n')