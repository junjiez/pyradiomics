#include <stdlib.h>
#include "cmatrices.h"

int calculate_glcm(int *image, char *mask, int Sx, int Sy, int Sz, int *angles, int Na, double *glcm, int Ng)
{
  /* Calculate GLCM: Count the number of voxels with gray level i is neighboured by a voxel with gray level j in
  *  direction and distance specified by a. Returns an asymmetrical GLCM matrix for each angle/distance
  *  defined in angles.
  */
  int glcm_idx_max = Ng * Ng * Na;
  int i = 0, j = 0;
  int iz, iy, ix;
  int a, glcm_idx;
  for (iz = 0; iz < Sz; iz ++) // Iterate over all voxels in a row - column - slice order
  {
    for (iy = 0; iy < Sy; iy ++)
    {
      for (ix = 0; ix < Sx; ix ++)
      {
        if (mask[i])  // Check if the current voxel is part of the segmentation
        {
          for (a = 0; a < Na; a++)  // Iterate over angles to get the neighbours
          {
            // Check whether the neighbour index is not out of range (i.e. part of the image)
            if (iz + angles[a * 3] >= 0 && iz + angles[a * 3] < Sz &&
                iy + angles[a * 3 + 1] >= 0 && iy + angles[a * 3 + 1] < Sy &&
                ix + angles[a * 3 + 2] >= 0 && ix + angles[a * 3 + 2] < Sx)
            {
              j = i + angles[a * 3 + 2] + angles[a * 3 + 1] * Sx + angles[a * 3] * Sy * Sx;
              if (mask[j])  // Check whether neighbour voxel is part of the segmentation
              {
                glcm_idx = a + (image[j]-1) * Na + (image[i]-1) * Na * Ng;
                if (glcm_idx >= glcm_idx_max) return 0; // Index out of range
                glcm[glcm_idx] ++;
                }
            }
          }
        }
        // Increase index to point to next item. Only one index is needed as data is stored one dimensionally in memory.
        // This is in row-column-slice order.
        i++;
      }
    }
  }
  return 1;
}

int calculate_glszm(int *image, char *mask, int Sx, int Sy, int Sz, int *angles, int Na, int *tempData, int Ng, int Ns)
{
  int Ni = Sz * Sy * Sx;
  int maxSize = 0;

  int *regionStack;
  int stackTop = -1;

  int regionCounter = 0;
  int max_region_idx = Ns * 2;

  int gl, region;
  int i = 0;
  int d, a;
  int cur_idx, cur_x, cur_y, cur_z, j;

  regionStack = (int *)calloc(Ns, sizeof(int));

  for (i = 0; i < Ni; i++)  // Loop over all the voxels in the image
  {
    // Check if the current voxel is part of the segmentation and unprocessed
    if (mask[i])
    {
      // Store current gray level, needed for region growing and determines index in GLSZM.
      gl = image[i];

      // Instantiate variable to hold region size at 0. Region increases for every found voxel.
      region = 0;

      // Start growing the region
      regionStack[++stackTop] = i; // Add the current voxel to the stack as 'starting point'
      mask[i] = 0;  // Mark current voxel as 'processed'

      while (stackTop > -1)
      {
        cur_idx = regionStack[stackTop--];  // Get the next voxel to process, on first iteration, this equals i

        // Increment region size, as number of loops corresponds to number of voxels in current region
        region++;

        // Generate neighbours for current voxel
        cur_z = (cur_idx / (Sx * Sy));
        cur_y = (cur_idx % (Sx * Sy)) / Sx;
        cur_x = (cur_idx % (Sx * Sy)) % Sx;
        for (d = 1; d >= -1; d -= 2) // Iterate over both directions for each angle
        {
          for (a = 0; a < Na; a++)  // Iterate over angles to get the neighbours
          {
            // Check whether the neighbour index is not out of range (i.e. part of the image)
            if (cur_z + d * angles[a * 3] >= 0 && cur_z + d * angles[a * 3] < Sz &&
              cur_y + d * angles[a * 3 + 1] >= 0 && cur_y + d * angles[a * 3 + 1] < Sy &&
              cur_x + d * angles[a * 3 + 2] >= 0 && cur_x + d * angles[a * 3 + 2] < Sx)
            {
              j = cur_idx + d * angles[a * 3 + 2] +
                d * angles[a * 3 + 1] * Sx +
                d * angles[a * 3] * Sy * Sx;
              // Check whether neighbour voxel is part of the current region and unprocessed
              if (mask[j] && (image[j] == gl))
              {
                // Push the voxel index to the stack for further processing
                regionStack[++stackTop] = j;
                // Voxel belongs to current region, mark it as 'processed'
                mask[j] = 0;
              }
            }
          } // next a
        } // next d
      }

      if (regionCounter >= max_region_idx)
      {
        free(regionStack);
        return -1; // index out of range
      }
      if (region > maxSize) maxSize = region;

      tempData[(regionCounter * 2)] = gl;
      tempData[((regionCounter * 2) + 1)] = region;

      regionCounter++;
    }
  }
  free(regionStack);

  if (regionCounter >= max_region_idx) return -1; // index out of range
  tempData[(regionCounter * 2)] = -1; // Set the first element after last region to -1 to stop the loop in fill_glszm
  return maxSize;
}

int fill_glszm(int *tempData, double *glszm, int Ng, int maxRegion)
{
  int i = 0;
  int glszm_idx_max = Ng * maxRegion;
  int glszm_idx;

  while(tempData[i * 2] > -1)
  {
    glszm_idx = (tempData[i * 2] - 1) * maxRegion + tempData[i * 2 + 1] - 1;
    if (glszm_idx >= glszm_idx_max) return 0; // Index out of range

    glszm[glszm_idx]++;
    i++;
  }
  return 1;
}

int calculate_glrlm(int *image, char *mask, int *size, int *strides, int *angles, int Na, double *glrlm, int Ng, int Nr)
{
  int a, da, nd;
  int mDims[3], sDims[2];
  int d1, d2;
  int jd[3];
  int glrlm_idx_max = Ng * Nr * Na;
  int runVal;
  char multiElement;

  for (a = 0; a < Na; a++)  // Iterate over angles to get the neighbours
  {
    multiElement = 0;
    nd = 0;
    for (da = 0; da < 3; da++)
    {
      if (angles[a * 3 + da] == 0)
      {
        sDims[da - nd] = da;
      }
      else
      {
        mDims[nd] = da;
        nd++;
      }
    }
    if (nd == 1)  // Moving in 1 dimension
    {
      // Iterate over all start voxels: All voxels, where moving dim == 0 or max (i.e. all combinations of voxels
      // in static dimensions)
      for (d1 = 0; d1 < size[sDims[0]]; d1++)
      {
        for (d2 = 0; d2 < size[sDims[1]]; d2++)
        {
          jd[sDims[0]] = d1;
          jd[sDims[1]] = d2;
          if (angles[a * 3 + mDims[0]]< 0)
          {
            jd[mDims[0]] = size[mDims[0]] - 1; // Moving dim is negative, set start at maximum index
          }
          else
          {
            jd[mDims[0]] = 0; // Moving dim is positive, set start at 0
          }
          // run_diagonal returns -1 if error, otherwise number of elements encountered
          runVal = run_diagonal(image, mask, size, strides, angles, Na, glrlm, glrlm_idx_max, Nr, jd, a);
          if (runVal < 0) return runVal; // index out of range, 'raise' error.
          if (!multiElement && runVal > 1) multiElement = 1; // multiple elements found
        }
      }
    }
    else if (nd == 2) // Moving in 2 dimensions
    {
      for (d1 = 0; d1 < size[sDims[0]]; d1++) // iterate over all voxels in the static dimension
      {
        for (d2 = 0; d2 < size[mDims[0]]; d2++) // Iterate over moving dimension 1, treating it as a 'static'
        {
          jd[sDims[0]] = d1;
          jd[mDims[0]] = d2;
          if (angles[a * 3 + mDims[1]] < 0)
          {
            jd[mDims[1]] = size[mDims[1]] - 1; // Moving dim is negative, set start at maximum index
          }
          else
          {
            jd[mDims[1]] = 0; // Moving dim is positive, set start at 0
          }
          // run_diagonal returns -1 if error, otherwise number of elements encountered
          runVal = run_diagonal(image, mask, size, strides, angles, Na, glrlm, glrlm_idx_max, Nr, jd, a);
          if (runVal < 0) return runVal; // index out of range, 'raise' error.
          if (!multiElement && runVal > 1) multiElement = 1; // multiple elements found
        }
        for (d2 = 1; d2 < size[mDims[1]]; d2++) // Iterate over other moving dimension, treating it as a 'static'
        {
          jd[sDims[0]] = d1;
          jd[mDims[1]] = d2;
          // Prevent calculating the true diagonal twice, which is either at index 0, or max, depending on the
          // angle of the the second moving dimension: iterate 1 to max if angle is positive, 0 to max -1 if
          // negative. The ignored index is handled in the previous loop.
          if (angles[a * 3 + mDims[1]] < 0) jd[mDims[1]] --;
          if (angles[a * 3 + mDims[0]] < 0)
          {
            jd[mDims[0]] = size[mDims[0]] - 1; // Moving dim is negative, set start at maximum index
          }
          else
          {
            jd[mDims[0]] = 0; // Moving dim is positive, set start at 0
          }
          // run_diagonal returns -1 if error, otherwise number of elements encountered
          runVal = run_diagonal(image, mask, size, strides, angles, Na, glrlm, glrlm_idx_max, Nr, jd, a);
          if (runVal < 0) return runVal; // index out of range, 'raise' error.
          if (!multiElement && runVal > 1) multiElement = 1; // multiple elements found
        }
      }
    }
    else if (nd == 3)
    {
      for (d1 = 0; d1 < size[mDims[0]]; d1++)
      {
        for (d2 = 0; d2 < size[mDims[1]]; d2++)
        {
          jd[mDims[0]] = d1;
          jd[mDims[1]] = d2;
          if (angles[a * 3 + mDims[2]] < 0)
          {
            jd[mDims[2]] = size[mDims[2]] - 1; // Moving dim is negative, set start at maximum index
          }
          else
          {
            jd[mDims[2]] = 0; // Moving dim is positive, set start at 0
          }
          // run_diagonal returns -1 if error, otherwise number of elements encountered
          runVal = run_diagonal(image, mask, size, strides, angles, Na, glrlm, glrlm_idx_max, Nr, jd, a);
          if (runVal < 0) return runVal; // index out of range, 'raise' error.
          if (!multiElement && runVal > 1) multiElement = 1; // multiple elements found
        }
      }
      for (d1 = 0; d1 < size[mDims[1]]; d1++)
      {
        for (d2 = 1; d2 < size[mDims[2]]; d2++)
        {
          jd[mDims[1]] = d1;
          jd[mDims[2]] = d2;
          // Prevent calculating the true diagonal twice, which is either at index 0, or max, depending on the
          // angle of the the second moving dimension: iterate 1 to max if angle is positive, 0 to max -1 if
          // negative. The ignored index is handled in the previous loop.
          if (angles[a * 3 + mDims[2]] < 0) jd[mDims[2]] --;
          if (angles[a * 3 + mDims[0]] < 0)
          {
            jd[mDims[0]] = size[mDims[0]] - 1; // Moving dim is negative, set start at maximum index
          }
          else
          {
            jd[mDims[0]] = 0; // Moving dim is positive, set start at 0
          }
          // run_diagonal returns -1 if error, otherwise number of elements encountered
          runVal = run_diagonal(image, mask, size, strides, angles, Na, glrlm, glrlm_idx_max, Nr, jd, a);
          if (runVal < 0) return runVal; // index out of range, 'raise' error.
          if (!multiElement && runVal > 1) multiElement = 1; // multiple elements found
        }
      }
      for (d1 = 1; d1 < size[mDims[2]]; d1++)
      {
        for (d2 = 1; d2 < size[mDims[0]]; d2++)
        {
          jd[mDims[2]] = d1;
          jd[mDims[0]] = d2;
          // Prevent calculating the true diagonal twice, which is either at index 0, or max, depending on the
          // angle of the the second moving dimension: iterate 1 to max if angle is positive, 0 to max -1 if
          // negative. The ignored index is handled in the previous loops. This is done for both 'statics'.
          if (angles[a * 3 + mDims[2]] < 0) jd[mDims[2]] --;
          if (angles[a * 3 + mDims[0]] < 0) jd[mDims[0]] --;
          if (angles[a * 3 + mDims[1]] < 0)
          {
            jd[mDims[1]] = size[mDims[1]] - 1; // Moving dim is negative, set start at maximum index
          }
          else
          {
            jd[mDims[1]] = 0; // Moving dim is positive, set start at 0
          }
          // run_diagonal returns -1 if error, otherwise number of elements encountered
          runVal = run_diagonal(image, mask, size, strides, angles, Na, glrlm, glrlm_idx_max, Nr, jd, a);
          if (runVal < 0) return runVal; // index out of range, 'raise' error.
          if (!multiElement && runVal > 1) multiElement = 1; // multiple elements found
        }
      }
    } // switch No of dimensions
    if (!multiElement) // Segmentation is 2D for this angle, remove it.
    {
      // set elements at runlength index 0 to 0 for all gray levels in this angle, other runlength indices are
      // already 0.
      for (d1 = 0; d1 < Ng; d1++)
      {
        glrlm[a + d1 * Na * Nr] = 0;
      }
    }
  } // next a
  return 1;
}

int run_diagonal(int *image, char *mask, int *size, int *strides, int *angles, int Na, double *glrlm, int glrlm_idx_max, int Nr, int *jd, int a)
{
  int j, gl, rl;
  int glrlm_idx;
  int elements = 0;

  gl = -1;
  rl = 0;

  j = jd[0] * strides[0] + jd[1] * strides[1] + jd[2] * strides[2];
  while (jd[0] >= 0 && jd[0] < size[0] &&
        jd[1] >= 0 && jd[1] < size[1] &&
        jd[2] >= 0 && jd[2] < size[2])
  {
    j = jd[0] * strides[0] + jd[1] * strides[1] + jd[2] * strides[2];
    if (mask[j])
    {
        elements++; // Count the number of segmented voxels in this run
      if (gl == image[j])
      {
        rl++;
      }
      else if (gl == -1)
      {
        gl = image[j];
        rl = 0;
      }
      else
      {
        glrlm_idx = a + rl * Na + (gl - 1) * Na * Nr;
        if (glrlm_idx >= glrlm_idx_max) return -1;
        glrlm[glrlm_idx]++;

        gl = image[j];
        rl = 0;
      }
    }
    else if (gl > -1)
    {
      glrlm_idx = a + rl * Na + (gl - 1) * Na * Nr;
      if (glrlm_idx >= glrlm_idx_max) return -1;
      glrlm[glrlm_idx]++;
      gl = -1;
    }
    jd[0] += angles[a * 3];
    jd[1] += angles[a * 3 + 1];
    jd[2] += angles[a * 3 + 2];
  }
  if (gl > -1)
  {
    glrlm_idx = a + rl * Na + (gl - 1) * Na * Nr;
    if (glrlm_idx >= glrlm_idx_max) return -1;
    glrlm[glrlm_idx]++;
  }
  return elements;
}
