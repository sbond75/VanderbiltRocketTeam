//
//  KeypointsAndMatching.cpp
//  SIFT
//
//  Created by VADL on 10/31/21.
//  Copyright © 2021 VADL. All rights reserved.
//

#include "KeypointsAndMatching.hpp"

int SIFTParams::call_params_function(const char *name, struct sift_parameters* p)
{
  int i;

  for (i = 0; i < (sizeof(function_map) / sizeof(function_map[0])); i++) {
    if (!strcmp(function_map[i].name, name) && function_map[i].func) {
      function_map[i].func(p);
      return 0;
    }
  }

  return -1;
}
void SIFTParams::print_params_functions() {
    int i;
    int max = (sizeof(function_map) / sizeof(function_map[0]));
    for (i = 0; i < max; i++) {
        printf("%s%s", function_map[i].name, i == max - 1 ? "" : ", ");
    }
}

SIFTParams::SIFTParams() {
//    /** assign parameters **/
//    params = sift_assign_default_parameters();
//    // //
}

SIFTParams::~SIFTParams() {
    // Cleanup
    //free(params); // FIXME: free
}

SIFTState::~SIFTState() {
    // Cleanup
    for (struct sift_keypoints* keypoints : computedKeypoints) {
        sift_free_keypoints(keypoints);
    }

    if (out_k1 != nullptr) {
        sift_free_keypoints(out_k1);
        sift_free_keypoints(out_k2A);
        sift_free_keypoints(out_k2B);
    }
}
