# multi_sense_wordvec
通过使用改进的GloVe模型训练多语义词向量

环境依赖：
python3

词义消岐需要安装pywsd https://github.com/alvations/pywsd

GloVe源码可参考 https://github.com/stanfordnlp/GloVe 或windows平台 vs2017 https://github.com/anoidgit/GloVe-win 

本实验代码均在win10下完成，其中多语义词向量训练使用visual studio 2017 平台

# 实验步骤
1、使用process_wiki.py文件处理原始wiki百科文件，生成标注语料库

  例如：
  
  python process_wiki.py F://paper//data//wiki2010/wiki2010.txt  F://paper//data//wiki2010/wiki_pwd.txt
  
  输入：原始wiki百科文件即F://paper//data//wiki2010/wiki2010.txt
  
  输出：区分不同语义标注文件F://paper//data//wiki2010/wiki_pwd.txt 。
  标注文件效果如下：
  
  ![Image text](https://github.com/reason519/crawl_bdwksp/blob/master/img/%E7%BD%91%E5%9D%80%E4%BF%AE%E6%94%B9%E4%BD%8D%E7%BD%AE.JPG)
  
2、

