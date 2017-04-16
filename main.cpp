#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <highgui.h>
#include <cv.h>
#include <windows.h>
#include <vector>
#include <math.h>
#include <FreeImage.h>
#include "rgbe.c"


using namespace std;


int PIX_MAX = 255;
int PIX_MIN = 0;
int PIX_MID = 127;
int Lamda = 50;
int rows ;
int cols ;
char **PicName;
float *Exposure;
int Img_Number;
int PicWidth;
int PicHeight;
int Weight[256];
int HowManyPoint;
uchar ***PointRGB; // [image][point][RGB]
CvMat A_Mat[3],x_Mat[3],b_Mat[3];
vector<CvPoint> PickPix;

void Settings();
void MouseSetPoint(int event,int x,int y,int flags,void *param);
void HDRSolve();
void SaveResponseCurve();

int main()
{  
	
   char FolderName[40]={};
   char temp[40] = {};
   printf("Please Enter Image Folder: ");
   gets(FolderName);
   strcpy(temp,FolderName);
   

   strcat(temp,"\\Img_Info.txt");

   FILE *fptr;
   fptr = fopen(temp,"rb");

   fscanf(fptr,"%d",&Img_Number);
   PicName = new char *[Img_Number];
   for(int i=0;i<Img_Number;i++)
   {PicName[i] = new char[40];}
   Exposure = new float[Img_Number];


   // image info
   for(int i=0;i<Img_Number;i++)
   {
	   memset(temp,'\0',sizeof(temp));
	   strcpy(temp,FolderName);
	   strcat(temp,"\\");
	   fscanf(fptr, "%s%f", PicName[i], &Exposure[i]);
	   strcat(temp,PicName[i]);
	   strcpy(PicName[i],temp);
	   printf("Picture: %s, Exposure: %f\n",PicName[i],Exposure[i]);
   }

   printf("\nHow many sample points you want to choose: ");
   cin >> HowManyPoint;

   rows = Img_Number*HowManyPoint+256;
   cols = HowManyPoint+256;

   PointRGB = new uchar **[Img_Number];
   for(int i=0;i<Img_Number;i++){
	   PointRGB[i] = new uchar*[HowManyPoint];
          for(int j=0;j<HowManyPoint;j++){
			  PointRGB[i][j] = new uchar[3];
		  }
   }

   // user set point for solving HDR
   Settings();
   HDRSolve();

   printf("\nHDR Image Has Been Created!\n\n");
   
   delete [] Exposure;
   delete [] PicName;
   delete [] PointRGB;
   system("pause");

   return 0;





}


void MouseSetPoint(int event, int x, int y, int flags, void *param)
{

	if(event == CV_EVENT_LBUTTONDOWN)
	{
	  PickPix.push_back(cvPoint(x,y));
	  printf("You choose (%d,%d) !\n" ,x,y);
	  if(HowManyPoint - PickPix.size()!=0)
	  printf("Still have to choose %d points ^^\n\n", HowManyPoint - PickPix.size());
	}
	if((HowManyPoint-PickPix.size())==0)
	{
		cvDestroyWindow("SetPoint");
	}
}

void Settings()
{
   IplImage *BasePic;
   BasePic = cvLoadImage(PicName[0]);
   PicHeight = BasePic->height;
   PicWidth = BasePic->width;
   cvNamedWindow("SetPoint",CV_WINDOW_NORMAL);
   cvSetMouseCallback("SetPoint",MouseSetPoint,NULL);
   cvShowImage("SetPoint",BasePic);
   cvWaitKey(0);

}


void HDRSolve()
{

   // set point RGB value
   for(int i=0;i<Img_Number;i++)
   {
	   IplImage *image = cvLoadImage(PicName[i],1);
	   
	   for(int j=0;j<HowManyPoint;j++)
	   {
		   PointRGB[i][j][2] = image->imageData[PickPix[j].y*image->widthStep + PickPix[j].x*3];
		   PointRGB[i][j][1] = image->imageData[PickPix[j].y*image->widthStep + PickPix[j].x*3+1];
		   PointRGB[i][j][0] = image->imageData[PickPix[j].y*image->widthStep + PickPix[j].x*3+2];   
	   }
	   cvReleaseImage(&image);
   }

   
   printf("\nImage data set complete\n\n");
   
   float *A[3],*x[3],*b[3];

   // set weight
   for(int i= PIX_MIN; i<=PIX_MAX;i++)
   {
	   if(i<=PIX_MID)
	   {Weight[i] = i - PIX_MIN; }
	   else
	   {Weight[i] = PIX_MAX - i;}
   }

   for(int i=0;i<3;i++)
   {
     A[i] = new float[rows*cols]();
	 x[i] = new float[cols]();
	 b[i] = new float[rows]();


	 A[i][Img_Number*HowManyPoint*cols +PIX_MID-1] = (float)Weight[PIX_MID]; 
	 for(int j=0;j<HowManyPoint;j++){
		 for(int k=0;k<Img_Number;k++)
		 {
			 A[i][(j*Img_Number+k)*cols+PointRGB[k][j][i]] = (float)Weight[PointRGB[k][j][i]];
			 A[i][(j*Img_Number+k)*cols+255+j] = -1.0*Weight[PointRGB[k][j][i]];
			 b[i][Img_Number*j+k] = (float)Weight[PointRGB[k][j][i]]*log(Exposure[k]);
		 }
	 }

	 int shift =0;
	 for(int z=0;z<254;z++){
		A[i][(Img_Number*HowManyPoint+1+z)*cols+shift] = (float)Weight[z+1]*Lamda;
		A[i][(Img_Number*HowManyPoint+1+z)*cols+shift+1] = (float)-2*Weight[z+1]*Lamda;
		A[i][(Img_Number*HowManyPoint+1+z)*cols+shift+2] = (float)Weight[z+1]*Lamda;
		shift ++;
	 }

	
	 A_Mat[i] = cvMat(rows,cols,CV_32FC1,A[i]);
	 x_Mat[i] = cvMat(cols,1,CV_32FC1,x[i]);
	 b_Mat[i] = cvMat(rows,1,CV_32FC1,b[i]);
   }
	 for(int i=0;i<3;i++)
	 {
	   cvSolve(&A_Mat[i], &b_Mat[i], &x_Mat[i],CV_SVD);
	 }

	 SaveResponseCurve();

	 IplImage **Images = new IplImage *[Img_Number];
	 CvSize ImageSize = cvSize(PicWidth,PicHeight);

	 float *HDRImage = new float[3*PicHeight*PicWidth];
	 float Energy = 0, Weight_sum = 0;

	 for(int i=0;i<Img_Number;i++)
	 {
		 Images[i] = cvLoadImage(PicName[i],1);
	 }


	 for(int k=0;k<3;k++)
	 {
		 for(int i=0;i<PicHeight;i++){
			 for(int j=0;j<PicWidth;j++)
			 {
				 Energy = 0, 
			     Weight_sum = 0;
				 for(int n=0;n<Img_Number;n++)
				 {
					 uchar index = Images[n]->imageData[i*Images[n]->widthStep+j*3+2-k];
					 Weight_sum = Weight_sum + Weight[index];
					 Energy = Energy + (float)(Weight[index]*(cvGetReal1D(&x_Mat[k],index)-Exposure[n]));
				 }
				 HDRImage[i*PicWidth*3+j*3+k] = exp(Energy/Weight_sum);
			 }
		 }
	 }

	 FILE *f;
	 f = fopen("hdr_result.hdr","wb");
	 RGBE_WriteHeader(f,PicWidth,PicHeight,NULL);
	 RGBE_WritePixels(f,HDRImage,PicWidth*PicHeight);
     fclose(f);

	 
	 
	 
	 
	 
	 
	 }

   
void SaveResponseCurve()
{
	 FILE *fptr;
	 fptr = fopen("R.txt","w");
	 for(int i=PIX_MIN;i<=PIX_MAX;i++)
		 fprintf(fptr,"%f\n",cvGetReal1D(&x_Mat[0],i));
	 fclose(fptr);

	 fptr = fopen("G.txt","w");
	 for(int i=PIX_MIN;i<=PIX_MAX;i++)
		 fprintf(fptr,"%f\n",cvGetReal1D(&x_Mat[1],i));
	 fclose(fptr);

	 fptr = fopen("B.txt","w");
	 for(int i=PIX_MIN;i<=PIX_MAX;i++)
		 fprintf(fptr,"%f\n",cvGetReal1D(&x_Mat[2],i));
	 fclose(fptr);




}