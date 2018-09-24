#!/bin/bash

ps aux | grep "main.py check" | awk '{print $2}' | xargs kill
ps aux | grep "KSym.so -KSym" | awk '{print $2}' | xargs kill
