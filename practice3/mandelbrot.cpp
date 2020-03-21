// Пункт №7

#include <stdio.h>
#include <math.h>
#include <omp.h>
#include <iostream>
#include <chrono>

using namespace std;

const int iXmax = 5000; 
const int iYmax = 5000;

unsigned char imgBuf[iYmax][iXmax][3];

 int main()
{  
    /* world ( double) coordinate = parameter plane*/
    double Cx;
    double Cy;
    
    const double CxMin = -2.5;
    const double CxMax =  1.5;
    const double CyMin = -2.0;
    const double CyMax =  2.0;
    
    double PixelWidth  = (CxMax - CxMin) / iXmax;
    double PixelHeight = (CyMax - CyMin) / iYmax;
    
    /* color component ( R or G or B) is coded from 0 to 255 */
    /* it is 24 bit color RGB file */
    const int MaxColorComponentValue = 255; 
    
    FILE * fp;
    char *filename = "new1.ppm";
    char *comment = "# ";/* comment should start with # */
    
    static unsigned char color[3];
    
    double Zx;
    double Zy;
    
    double Zx2;
    double Zy2;

    int Iteration;
    const int IterationMax = 200;
    
    /* bail-out value , radius of circle ;  */
    const double EscapeRadius = 2;
    double ER2 = EscapeRadius * EscapeRadius;
    
    /*create new file,give it a name and open it in binary mode  */
    fp = fopen(filename, "wb"); /* b -  binary mode */
    
    /*write ASCII header to the file*/
    fprintf(fp, "P6\n %s\n %d\n %d\n %d\n", comment, iXmax, iYmax, MaxColorComponentValue);
    
    auto begin = chrono::steady_clock::now();

    /* compute and write image data bytes to the file*/
    #pragma omp parallel for private(Cy, Cx, Zx, Zy, Zx2, Zy2, Iteration) \
            num_threads(1) schedule(dynamic, 50)
    for (int iY = 0; iY < iYmax; iY++)
    {
        Cy = CyMin + iY * PixelHeight;
        if (fabs(Cy) < PixelHeight / 2)
            Cy = 0.0; /* Main antenna */
        
        for (int iX = 0; iX < iXmax; iX++)
        {         
            Cx = CxMin + iX * PixelWidth;
            
            /* initial value of orbit = critical point Z= 0 */
            Zx = 0.0;
            Zy = 0.0;

            Zx2 = Zx * Zx;
            Zy2 = Zy * Zy;
            
            for (Iteration = 0; Iteration < IterationMax && ((Zx2 + Zy2) < ER2); Iteration++)
            {
                Zy = 2 * Zx * Zy + Cy;
                Zx = Zx2 - Zy2 + Cx;
                
                Zx2 = Zx * Zx;
                Zy2 = Zy * Zy;
            }

            /* compute  pixel color (24 bit = 3 bytes) */
            if (Iteration == IterationMax)
            { 
                /*  interior of Mandelbrot set = black */
                imgBuf[iY][iX][0] = 0;
                imgBuf[iY][iX][1] = 0;
                imgBuf[iY][iX][2] = 0;                        
            }
            else 
            { 
                /* exterior of Mandelbrot set = white */
                imgBuf[iY][iX][0] = 255;
                imgBuf[iY][iX][1] = 255;
                imgBuf[iY][iX][2] = 255; 

            }
        }
    }

    auto end = chrono::steady_clock::now();
    
    auto elapsed_time = chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    cout << "Calculations was running " << elapsed_time.count() << " ms" <<  endl;
    
    fwrite(imgBuf, 1, iXmax * iYmax * 3, fp);

    fclose(fp);
    
    return 0;
 }