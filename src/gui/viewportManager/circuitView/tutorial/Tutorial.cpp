#include <stdlib.h>
#include <stdio.h>
#include "Tutorial.h"

bool condition_satisfied;
std::unqique_ptr<Tutorial> next_state;
Tutorial::Tutorial(){
}
void Tutorial::t(){
    newCircuit();
    const char* filePath = "/Users/yuguang/Documents/test.cir";                                                                                                                                   
    const char* const* = &filepath;
    LoadCallback(this, filePath, -1);
   
}

void Tutorial::load_next(){
}

void Tutorial::next(){
    if (condition_satisfied){
        load_next;
    }
}



