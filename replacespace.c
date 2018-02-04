void remove_spaces(){      
  char TEMP_OBJECT[256]; //Copy requested object to a temp buffer
  for (int c = 0; c < strlen(REQ_OBJECT); ++c) {
      TEMP_OBJECT[c] = REQ_OBJECT[c];
  }

  int numSpaces=0,i=0; //count the number of spaces in the file name
  while(i+2<strlen(TEMP_OBJECT)){
    if (TEMP_OBJECT[i]=='%' && TEMP_OBJECT[i+1]=='2' && TEMP_OBJECT[i+2]=='0') {
      numSpaces++;
      i+=3;
    }
    else
      i++;
  }

  if (numSpaces!=0){ //if there is a least one space char
    int it=0,j=0;
    while(it+2<strlen(TEMP_OBJECT)){
      if (TEMP_OBJECT[it]=='%' && TEMP_OBJECT[it+1]=='2' && TEMP_OBJECT[it+2]=='0') {//replace with ' '
        REQ_OBJECT[j] = ' ';
        it+=3;
        j++;
      }
      else{//copy as is
        REQ_OBJECT[j] = TEMP_OBJECT[it];
        it++;
        j++;
      }
    }
    while(it<strlen(TEMP_OBJECT)){ //copy remaining characters
      REQ_OBJECT[j] = TEMP_OBJECT[it];
        it++;
        j++;
    }
  }
}