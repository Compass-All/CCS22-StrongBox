#ifndef __NEAREST_NEIGHBOR__
#define __NEAREST_NEIGHBOR__

#include "nearestNeighbor.h"
#include "timing.h"

#include "strongbox_lib.h"

#ifdef TIMING
#define PROFILING

struct timeval tv;
struct timeval tv_total_start, tv_total_end;
struct timeval tv_init_end;
struct timeval tv_h2d_start, tv_h2d_end;
struct timeval tv_d2h_start, tv_d2h_end;
struct timeval tv_kernel_start, tv_kernel_end;
struct timeval tv_mem_alloc_start, tv_mem_alloc_end;
struct timeval tv_close_start, tv_close_end;
float init_time = 0, mem_alloc_time = 0, h2d_time = 0, kernel_time = 0,
      d2h_time = 0, close_time = 0, total_time = 0;
#endif
char *data_folder;

cl_context context=NULL;

int main(int argc, char *argv[]) {
  mark_secure();
  std::vector<Record> records;
  float *recordDistances;
  //LatLong locations[REC_WINDOW];
  std::vector<LatLong> locations;
  int i;
  // args
  char filename[100];
  int resultsCount=10,quiet=0,timing=0,platform=-1,device=-1;
  float lat=0.0,lng=0.0;
  
  // parse command line
  if (parseCommandline(argc, argv, filename,&resultsCount,&lat,&lng,
                     &quiet, &timing, &platform, &device)) {
    printUsage();
    return 0;
  }
  
  int numRecords = loadData(filename,records,locations);

  // ========== //
	// The encryption in here is just for development test. You should prepare your encrypted data in advance. 
	aes_enc((void*)&locations[0], sizeof(LatLong) * numRecords);
// ========== //
  
  //for(i=0;i<numRecords;i++)
  //    printf("%s, %f, %f\n",(records[i].recString),locations[i].lat,locations[i].lng);


  if (!quiet) {
    printf("Number of records: %d\n",numRecords);
    printf("Finding the %d closest neighbors.\n",resultsCount);
  }

  if (resultsCount > numRecords) resultsCount = numRecords;

#ifdef  TIMING
  gettimeofday(&tv_total_start, NULL);
#endif
  context = cl_init_context(platform,device,quiet);
#ifdef  TIMING
	gettimeofday(&tv_init_end, NULL);
	tvsub(&tv_init_end, &tv_total_start, &tv);
	init_time = tv.tv_sec * 1000.0 + (float) tv.tv_usec / 1000.0;
#endif

  recordDistances = OpenClFindNearestNeighbors(context,numRecords,locations,lat,lng,timing);

  // find the resultsCount least distances
  findLowest(records,recordDistances,numRecords,resultsCount);

  // print out results
  if (!quiet)
    for(i=0;i<resultsCount;i++) {
      printf("%s --> Distance=%f\n",records[i].recString,records[i].distance);
    }
  free(recordDistances);
  return 0;
}

float *OpenClFindNearestNeighbors(
	cl_context context,
	int numRecords,
	std::vector<LatLong> &locations,float lat,float lng,
	int timing) {

    // 1. set up kernel
    cl_kernel NN_kernel;
        cl_int status;
        cl_program cl_NN_program;
        cl_NN_program = cl_compileProgram(
            (char *)"nearestNeighbor_kernel.cl",NULL);
       
        NN_kernel = clCreateKernel(
            cl_NN_program, "NearestNeighbor", &status);
        status = cl_errChk(status, (char *)"Error Creating Nearest Neighbor kernel",true);
        if(status)exit(1);
    // 2. set up memory on device and send ipts data to device
    // copy ipts(1,2) to device
    // also need to alloate memory for the distancePoints
    cl_mem d_locations;
    cl_mem d_distances;

    cl_int error=0;

  // ========== //
    create_operation_buffer(context);
  // ========== //

#ifdef  TIMING
    gettimeofday(&tv_mem_alloc_start, NULL);
#endif
    d_locations = clCreateAlignBuffer(context, CL_MEM_READ_ONLY,
        sizeof(LatLong) * numRecords, NULL, &error);

    d_distances = clCreateAlignBuffer(context, CL_MEM_READ_WRITE,
        sizeof(float) * numRecords, NULL, &error);
#ifdef  TIMING
    gettimeofday(&tv_mem_alloc_end, NULL);
    tvsub(&tv_mem_alloc_end, &tv_mem_alloc_start, &tv);
    mem_alloc_time = tv.tv_sec * 1000.0 + (float) tv.tv_usec / 1000.0;
#endif

    cl_command_queue command_queue = cl_getCommandQueue();
    cl_event writeEvent,kernelEvent,readEvent;
    error = clEnqueueWriteBuffer(command_queue,
               d_locations,
               1, // change to 0 for nonblocking write
               0, // offset
               sizeof(LatLong) * numRecords,
               &locations[0],
               0,
               NULL,
               &writeEvent);

    // 3. send arguments to device
    cl_int argchk;
    argchk  = clSetKernelArg(NN_kernel, 0, sizeof(cl_mem), (void *)&d_locations);
    argchk |= clSetKernelArg(NN_kernel, 1, sizeof(cl_mem), (void *)&d_distances);
    argchk |= clSetKernelArg(NN_kernel, 2, sizeof(int), (void *)&numRecords);
    argchk |= clSetKernelArg(NN_kernel, 3, sizeof(float), (void *)&lat);
    argchk |= clSetKernelArg(NN_kernel, 4, sizeof(float), (void *)&lng);

  // ========== //
    // d_locations is input buffer, just erase it after computation
    write_buffer_operation(d_locations, 1, 2);
    // d_distances is output buffer, which do not have any meaningful data at the beginning, we do not need to decrypt it before computation
    write_buffer_operation(d_distances, 2, 1);
    operation_end(command_queue);
  // ========== //

    cl_errChk(argchk,"ERROR in Setting Nearest Neighbor kernel args",true);

    // 4. enqueue kernel
    size_t globalWorkSize[1];
    globalWorkSize[0] = numRecords;
    if (numRecords % 64) globalWorkSize[0] += 64 - (numRecords % 64);
    //printf("Global Work Size: %zu\n",globalWorkSize[0]);      

    error = clEnqueueNDRangeKernel(
        command_queue,  NN_kernel, 1, 0,
        globalWorkSize,NULL,
        0, NULL, &kernelEvent);

    cl_errChk(error,"ERROR in Executing Kernel NearestNeighbor",true);
    tzasc_for_all(command_queue);

    // 5. transfer data off of device
    
    // create distances std::vector
    float *distances = (float *)malloc(sizeof(float) * numRecords);

    error = clEnqueueReadBuffer(command_queue,
        d_distances,
        1, // change to 0 for nonblocking write
        0, // offset
        sizeof(float) * numRecords,
        distances,
        0,
        NULL,
        &readEvent);
    cl_errChk(error,"ERROR with clEnqueueReadBuffer",true);
    clFinish(command_queue);

    
    if (timing) {
#ifdef TIMING
        clFinish(command_queue);
        cl_ulong eventStart,eventEnd,totalTime=0;
        printf("# Records\tWrite(s) [size]\t\tKernel(s)\tRead(s)  [size]\t\tTotal(s)\n");
        printf("%d        \t",numRecords);
        // Write Buffer
        error = clGetEventProfilingInfo(writeEvent,CL_PROFILING_COMMAND_START,
                                        sizeof(cl_ulong),&eventStart,NULL);
        cl_errChk(error,"ERROR in Event Profiling (Write Start)",true); 
        error = clGetEventProfilingInfo(writeEvent,CL_PROFILING_COMMAND_END,
                                        sizeof(cl_ulong),&eventEnd,NULL);
        cl_errChk(error,"ERROR in Event Profiling (Write End)",true);

        printf("%f [%.2fMB]\t",(float)((eventEnd-eventStart)/1e9),(float)((sizeof(LatLong) * numRecords)/1e6));
        totalTime += eventEnd-eventStart;
		h2d_time = (eventEnd - eventStart) / 1e6;

        // Kernel
        error = clGetEventProfilingInfo(kernelEvent,CL_PROFILING_COMMAND_START,
                                        sizeof(cl_ulong),&eventStart,NULL);
        cl_errChk(error,"ERROR in Event Profiling (Kernel Start)",true); 
        error = clGetEventProfilingInfo(kernelEvent,CL_PROFILING_COMMAND_END,
                                        sizeof(cl_ulong),&eventEnd,NULL);
        cl_errChk(error,"ERROR in Event Profiling (Kernel End)",true);

        printf("%f\t",(float)((eventEnd-eventStart)/1e9));
        totalTime += eventEnd-eventStart;
		kernel_time = (eventEnd - eventStart) / 1e6;

        // Read Buffer
        error = clGetEventProfilingInfo(readEvent,CL_PROFILING_COMMAND_START,
                                        sizeof(cl_ulong),&eventStart,NULL);
        cl_errChk(error,"ERROR in Event Profiling (Read Start)",true); 
        error = clGetEventProfilingInfo(readEvent,CL_PROFILING_COMMAND_END,
                                        sizeof(cl_ulong),&eventEnd,NULL);
        cl_errChk(error,"ERROR in Event Profiling (Read End)",true);

        printf("%f [%.2fMB]\t",(float)((eventEnd-eventStart)/1e9),(float)((sizeof(float) * numRecords)/1e6));
        totalTime += eventEnd-eventStart;
		d2h_time = (eventEnd - eventStart) / 1e6;

        printf("%f\n\n",(float)(totalTime/1e9));
#endif
    }

    // 6. return finalized data and release buffers
#ifdef  TIMING
	gettimeofday(&tv_close_start, NULL);
#endif
    clReleaseMemObject(d_locations);
    clReleaseMemObject(d_distances);
#ifdef  TIMING
	gettimeofday(&tv_close_end, NULL);
	tvsub(&tv_close_end, &tv_close_start, &tv);
	close_time = tv.tv_sec * 1000.0 + (float) tv.tv_usec / 1000.0;
	tvsub(&tv_close_end, &tv_total_start, &tv);
	total_time = tv.tv_sec * 1000.0 + (float) tv.tv_usec / 1000.0;

	printf("Init: %f\n", init_time);
	printf("MemAlloc: %f\n", mem_alloc_time);
	printf("HtoD: %f\n", h2d_time);
	printf("Exec: %f\n", kernel_time);
	printf("DtoH: %f\n", d2h_time);
	printf("Close: %f\n", close_time);
	printf("Total: %f\n", total_time);
#endif
// ========== //
	// The decryption in here is just for development test. 
  aes_dec((void*)&distances[0], sizeof(float) * numRecords);
// ========== //

  FILE* f = fopen("nn_result.txt", "w");
  for (int i = 0 ; i < numRecords ; ++ i)
    fprintf(f, "%x\n", *(unsigned*)&distances[i]);
  fclose(f);

	return distances;
}

int loadData(char *filename,std::vector<Record> &records,std::vector<LatLong> &locations){
    FILE   *flist,*fp;
	int    i=0;
	char dbname[64], db_fullname[128];
	int recNum=0;
	
    /**Main processing **/
    
    flist = fopen(filename, "r");
	while(!feof(flist)) {
		/**
		* Read in REC_WINDOW records of length REC_LENGTH
		* If this is the last file in the filelist, then done
		* else open next file to be read next iteration
		*/
		if(fscanf(flist, "%s\n", dbname) != 1) {
            fprintf(stderr, "error reading filelist\n");
            exit(0);
        }
        sprintf(db_fullname, "%s/%s", data_folder, dbname);
        fp = fopen(db_fullname, "r");
        if(!fp) {
            printf("error opening a db (%s)\n", db_fullname);
            exit(1);
        }
        // read each record
        while(!feof(fp)){
            Record record;
            LatLong latLong;
            fgets(record.recString,49,fp);
            fgetc(fp); // newline
            if (feof(fp)) break;
            
            // parse for lat and long
            char substr[6];
            
            for(i=0;i<5;i++) substr[i] = *(record.recString+i+28);
            substr[5] = '\0';
            latLong.lat = atof(substr);
            
            for(i=0;i<5;i++) substr[i] = *(record.recString+i+33);
            substr[5] = '\0';
            latLong.lng = atof(substr);
            
            locations.push_back(latLong);
            records.push_back(record);
            recNum++;
        }
        fclose(fp);
    }
    fclose(flist);
    return recNum;
}

void findLowest(std::vector<Record> &records,float *distances,int numRecords,int topN){
  int i,j;
  float val;
  int minLoc;
  Record *tempRec;
  float tempDist;
  
  for(i=0;i<topN;i++) {
    minLoc = i;
    for(j=i;j<numRecords;j++) {
      val = distances[j];
      if (val < distances[minLoc]) minLoc = j;
    }
    // swap locations and distances
    tempRec = &records[i];
    records[i] = records[minLoc];
    records[minLoc] = *tempRec;
    
    tempDist = distances[i];
    distances[i] = distances[minLoc];
    distances[minLoc] = tempDist;
    
    // add distance to the min we just found
    records[i].distance = distances[i];
  }
}

int parseCommandline(int argc, char *argv[], char* filename,int *r,float *lat,float *lng,
                     int *q, int *t, int *p, int *d){
    int i;
    if (argc < 2) return 1; // error
    strncpy(filename,argv[1],100);
    char flag;
    
    for(i=1;i<argc;i++) {
      if (argv[i][0]=='-') {// flag
        flag = argv[i][1];
          switch (flag) {
            case 'r': // number of results
              i++;
              *r = atoi(argv[i]);
              break;
            case 'l': // lat or lng
              if (argv[i][2]=='a') {//lat
                *lat = atof(argv[i+1]);
              }
              else {//lng
                *lng = atof(argv[i+1]);
              }
              i++;
              break;
            case 'h': // help
              return 1;
              break;
            case 'q': // quiet
              *q = 1;
              break;
            case 't': // timing
              *t = 1;
              break;
            case 'p': // platform
              i++;
              *p = atoi(argv[i]);
              break;
            case 'd': // device
              i++;
              *d = atoi(argv[i]);
              break;
            case 'f': // data folder
              i++;
              data_folder = argv[i];
              break;
        }
      }
    }
    if ((*d >= 0 && *p<0) || (*p>=0 && *d<0)) // both p and d must be specified if either are specified
      return 1;
    return 0;
}

void printUsage(){
  printf("Nearest Neighbor Usage\n");
  printf("\n");
  printf("nearestNeighbor [filename] -r [int] -lat [float] -lng [float] [-hqt] [-p [int] -d [int]]\n");
  printf("\n");
  printf("example:\n");
  printf("$ ./nearestNeighbor filelist.txt -r 5 -lat 30 -lng 90\n");
  printf("\n");
  printf("filename     the filename that lists the data input files\n");
  printf("-r [int]     the number of records to return (default: 10)\n");
  printf("-lat [float] the latitude for nearest neighbors (default: 0)\n");
  printf("-lng [float] the longitude for nearest neighbors (default: 0)\n");
  printf("\n");
  printf("-h, --help   Display the help file\n");
  printf("-q           Quiet mode. Suppress all text output.\n");
  printf("-t           Print timing information.\n");
  printf("\n");
  printf("-p [int]     Choose the platform (must choose both platform and device)\n");
  printf("-d [int]     Choose the device (must choose both platform and device)\n");
  printf("\n");
  printf("\n");
  printf("Notes: 1. The filename is required as the first parameter.\n");
  printf("       2. If you declare either the device or the platform,\n");
  printf("          you must declare both.\n\n");
}

#endif

