# multi_sense_wordvec
通过使用改进的GloVe模型训练多语义词向量

环境依赖：
python3

词义消岐需要安装pywsd https://github.com/alvations/pywsd

GloVe源码可参考 https://github.com/stanfordnlp/GloVe 或windows平台 vs2017 https://github.com/anoidgit/GloVe-win 

本实验代码均在win10下完成，其中多语义词向量训练使用visual studio 2017 平台

# 实验步骤
## 预处理
1、使用process_wiki.py文件处理原始wiki百科文件，生成标注语料库

  例如：
  
  python process_wiki.py F://paper//data//wiki2010/wiki2010.txt  F://paper//data//wiki2010/wiki_pwd.txt
  
  输入：原始wiki百科文件即F://paper//data//wiki2010/wiki2010.txt
  
  输出：区分不同语义标注文件F://paper//data//wiki2010/wiki_pwd.txt 
  
  标注文件效果如下：
  
  ![Image text](https://github.com/reason519/multi_sense_wordvec/blob/master/img/%E5%8E%9F%E5%A7%8B%E6%96%87%E6%9C%AC%E8%AF%8D%E4%B9%89%E6%B6%88%E5%B2%90%E7%BB%93%E6%9E%9C%E7%A4%BA%E4%BE%8B.JPG)
  
2、使用gen_voc.py生成所有词语的词典

  例如：
  
  python gen_voc.py F://paper//data//wiki2010/wiki_pwd.txt context_voc_all.txt  sense_voc_all.txt  word_to_sense_all.txt
  
  输入：步骤1中生成标注文件 F://paper//data//wiki2010/wiki_pwd.txt
  
  输出：
  
      词语词典：context_voc_all.txt
      
      语义项词典：sense_voc_all.txt
      
      词语与语义项之间的对应关系：word_to_sense_all.txt
      
3、过滤低频词 保留常用的40w个单词(process_voc_40w.py)

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
    
## 使用改进GloVe训练多语义词向量

1、使用修改后的cooccur.c生成共现矩阵
   
   cooccur.c文件中476-479行为词典、词义项和标注维基百科输入路径（可根据需要进行修改）
   
   ![Image text](https://github.com/reason519/multi_sense_wordvec/blob/master/img/%E5%8E%9F%E5%A7%8B%E6%96%87%E6%9C%AC%E8%AF%8D%E4%B9%89%E6%B6%88%E5%B2%90%E7%BB%93%E6%9E%9C%E7%A4%BA%E4%BE%8B.JPG)
   
2、 shuff过程与原始GloVe模型一致，不需要任何代码修改，可参考https://github.com/stanfordnlp/GloVe/blob/master/src/shuffle.c

3、


