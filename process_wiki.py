'''
该文件用于将原始的维基百科文件，通过Lesk词义消岐方法生成区分不同语义的标注文件，
其中多义词和语义项之间使用‘|’分割，形成‘多义词|语义项’形式，区分多义词不同语义
便于使用改进的GloVe模型进行训练。
例如：
python process_wiki.py F://paper//data//wiki2010/wiki2010.txt  F://paper//data//wiki2010/wiki_pwd.txt
输入：原始wiki百科文件即F://paper//data//wiki2010/wiki2010.txt
输出：区分不同语义标注文件F://paper//data//wiki2010/wiki_pwd.txt 。
'''
import sys
import argparse

import nltk
from pywsd.lesk import cosine_lesk
from pywsd import disambiguate

class MySentences(object):
    def __init__(self, file_path):
        self.file_path = file_path

    def __iter__(self):
        with open(self.file_path,encoding='utf-8') as f:
            for line in f:
                tokenized_line = ' '.join(nltk.wordpunct_tokenize(line))
                is_alpha_word_line = [word for word in
                                                      tokenized_line.lower().split()
                                                      if word.isalpha()]
                yield is_alpha_word_line

def parse_arguments(argv):
    parser=argparse.ArgumentParser()
    parser.add_argument('file_path',type=str,help='Input the orginal text file ')
    parser.add_argument('pwd_file_path', type=str, help='output the  processed file ')
    return parser.parse_args(argv)

def main(args):
    file_path=args.file_path
    sentences = MySentences(file_path)
    # for i in sentences:
    #     print(i)
    with open(args.pwd_file_path,'w',encoding='utf-8') as f:
        for i in sentences:
            if len(i)>5:
                ls=[]
                try:
                    for  word_sen in disambiguate(' '.join(i),algorithm=cosine_lesk):
                        if word_sen[1] is None:
                            ls.append(word_sen[0])
                        else:
                            ls.append(word_sen[0]+'|'+word_sen[1].name())
                    f.write(' '.join(ls))
                    f.write('\n')
                except:
                    print(' '.join(i))

if __name__=='__main__':
    main(parse_arguments(sys.argv[1:]))
