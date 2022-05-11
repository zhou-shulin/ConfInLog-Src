#!/usr/bin/python
#-*-coding:utf-8-*-

from ast import parse
import os
import sys
import spacy

nlp = spacy.load('en_core_web_sm')

NOUN_TAG_LIST = ['NN', 'NNS', 'NNPS']
VERB_TAG_LIST = ['BES', 'HVS', 'VB', 'VBD', 'VBG', 'VBN', 'VBP', 'VBZ']
AUX_TAG_LIST = ['MD']
ADJ_TAG_LIST = ['JJ', 'JJR', 'JJS']
ADV_TAG_LIST = ['RB', 'RBR', 'RBS']


BE_VERB_LIST = ['is', 'are', 'am', 'be']
VERB_LIST = ['accept', 'require', 'support', 'allow', 'recommend', 'expect', 'take']    # take 1 argument
MODAL_LIST = ['should', 'must', 'need', 'can', 'cannot', 'have to', 'may', 'can\'t', 'could', 'could\'t']
ADJ_WORD_LIST = ['acceptable', 'valid', 'supportted', 'allowed', 'deprecated', 'ignored']
MD_MODE_VERB_LIST = ['be', 'have']
ADV_WORD_LIST = ['not']



def isConstraintDescription(conf_name, log_message):
    pos_seq = parse_pos_sequence(conf_name, log_message)
    # flag_match = False  # once unmatched, turn it to False
    cnt = 0
    Len = len(pos_seq)  # pos_seq:  [(lemma_, tag_, pos_), ...]
    while cnt < Len:
        if pos_seq[cnt][1] in AUX_TAG_LIST and pos_seq[cnt][0] in MODAL_LIST and \
            cnt+1<Len and pos_seq[cnt+1][1] in VERB_TAG_LIST and pos_seq[cnt+1][0] in MD_MODE_VERB_LIST:
            return True
        elif pos_seq[cnt][1] in AUX_TAG_LIST and pos_seq[cnt][0] in MODAL_LIST and \
            cnt+1<Len and pos_seq[cnt+1][1] in VERB_TAG_LIST and pos_seq[cnt+1][0] in VERB_LIST:
            return True
        elif pos_seq[cnt][1] in VERB_TAG_LIST and pos_seq[cnt][0] in VERB_LIST :
            return True
        elif pos_seq[cnt][1] in ADJ_TAG_LIST and pos_seq[cnt][0] in ADJ_WORD_LIST and \
            pos_seq[cnt+1][1] in NOUN_TAG_LIST :
            return True
        elif pos_seq[cnt][1] in VERB_TAG_LIST and pos_seq[cnt][0] in BE_VERB_LIST and \
            cnt+1<Len and pos_seq[cnt+1][1] in ADJ_TAG_LIST and pos_seq[cnt+1][0] in ADJ_WORD_LIST:
            return True
        elif pos_seq[cnt][1] in VERB_TAG_LIST and pos_seq[cnt][0] in BE_VERB_LIST and \
            cnt+1<Len and pos_seq[cnt+1][1] in ADV_TAG_LIST and pos_seq[cnt+1][0] in ADV_WORD_LIST and \
            cnt+2<Len and pos_seq[cnt+2][1] in ADJ_TAG_LIST and pos_seq[cnt+2][0] in ADJ_WORD_LIST:
            return True
        cnt += 1
    
    # if 'expected list of' in log_message:
    #     return True;

    return False


def parse_pos_sequence(conf_name, text):
    # print(text)

    text = text.replace(conf_name, 'CONFNAME')

    pos_seq = []
    doc = nlp(text)

    for token in doc:
        if token.tag_ == "DT" or token.tag_ == "SYM" or token.pos_ == "PUNCT":
            continue
        
        if token.lemma_ == conf_name:
            pos_seq.append((conf_name, "NN", "NOUN"))
            continue
            
        pos_seq.append((token.lemma_, token.tag_, token.pos_))
    # print(pos_seq)
    return pos_seq


def _parse_pos_sequence(text):
    print(text)
    pos_seq = []    
    doc = nlp(text)
    # print( (token.lemma_, token.tag_, token.pos) for token in doc)
    prev_token = doc[0]
    for token in doc:
            
        # if prev_token != token:
        #     print('prev token: ', prev_token.lemma_, prev_token.tag_, prev_token.pos_)
        # print('current token: ', token.lemma_, token.tag_, token.pos_)


        if token.tag_ == "DT" or token.tag_ == "SYM":
            continue
        # elif token.pos_ == "PUNCT" and ( token.tag_ == "." or token.tag_ == ","):
        elif token.pos_ == "PUNCT" :
            continue
        elif prev_token != token and token.tag_ == "NN" and prev_token.tag_ == "NN" or \
             token.tag_ == "JJ" and prev_token.tag_ == "JJ":
            pos_seq[-1] = (pos_seq[-1][0]+' '+token.lemma_, pos_seq[-1][1], pos_seq[-1][2])
            continue
        elif prev_token != token and token.lemma_ == "to" and prev_token.lemma_ == "have":
            pos_seq[-1] = (pos_seq[-1][0]+' '+token.lemma_, 'MD', 'AUX')
            continue

        
        prev_token = token
        pos_seq.append((token.lemma_, token.tag_, token.pos_))
    print(pos_seq)
    return pos_seq


if __name__ == '__main__':
    # pass
    text = 'AsyncRequestWorkerFactor argument must be a positive number.'
    _parse_pos_sequence(text)
    parse_pos_sequence('AsyncRequestWorkerFactor', text)
    text = 'mod_openssl.c s ssl.pemfile has to be set when ssl.engine = "enable" '
    _parse_pos_sequence(text)
    parse_pos_sequence('ssl.pemfile', text)