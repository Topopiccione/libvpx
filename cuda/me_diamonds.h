/*
    Cuda accelerated motion estimation for VP8 libvpx encoder
    by Pietro Paglierani, Giuliano Grossi, Federico Pedersini and Alessandro Petrini

    for Italtel and Universita' degli Studi di Milano
    2015-2016, Milano
*/

#ifndef _ME_DIAMONDS
#define _ME_DIAMONDS

#ifdef __cplusplus
extern "C" {
#endif


//extern __device__ __constant__ MV MV_16x12_lookup[];
//extern __device__ __constant__ MV_ref MV_lookup_refin_tex[];
//extern __device__ __constant__ MV MV_lookup_refin_fast[];


// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// | Shine on your crazy diamonds *
// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
/*
__device__ __constant__ MV_ref MV_lookup_refin_tex[] = { // Unit: pixel
        		                {-2.0, 0.0},
	{-1.0, -2.0}, {-1.0, -1.0}, {-1.0, 0.0}, {-1.0, 1.0}, {-1.0, 2.0},
		          { 0.0, -1.0}, { 0.0, 0.0}, { 0.0, 1.0},
	{1.0, -2.0},  { 1.0, -1.0}, { 1.0, 0.0}, { 1.0, 1.0},  {1.0, 2.0},
		                        { 2.0, 0.0},
		                        { 0.0, 0.0} };  // in piu', per arrivare a 16
*/
/*
__device__ __constant__ MV_ref MV_lookup_refin_tex[] = { // Unit: pixel
		                                                                            {-2.00, 0.00},
			                {-1.50,-1.00},              {-1.50,-0.50},              {-1.50, 0.00},              {-1.50, 0.50},              {-1.50, 1.00},
{-1.00,-2.00},{-1.00,-1.50},{-1.00,-1.00},              {-1.00,-0.50},              {-1.00, 0.00},              {-1.00, 0.50},              {-1.00, 1.00},{-1.00, 1.50},{-1.00, 2.00},
                                                                                    {-0.75, 0.00},
              {-0.50,-1.50},{-0.50,-1.00},              {-0.50,-0.50},              {-0.50, 0.00},              {-0.50, 0.50},              {-0.50, 1.00},{-0.50, 1.50},
                                                                      {-0.25,-0.25},{-0.25, 0.00},{-0.25, 0.25},
              { 0.00,-1.50},{ 0.00,-1.00},{ 0.00,-0.75},{ 0.00,-0.50},{ 0.00,-0.25},{ 0.00, 0.00},{ 0.00, 0.25},{ 0.00, 0.50},{ 0.00, 0.75},{ 0.00, 1.00},{ 0.00, 1.50},
                                                                      { 0.25,-0.25},{ 0.25, 0.00},{ 0.25, 0.25},
              { 0.50,-1.50},{ 0.50,-1.00},              { 0.50,-0.50},              { 0.50, 0.00},              { 0.50, 0.50},              { 0.50, 1.00},{ 0.50, 1.50},
                                                                                    { 0.75, 0.00},
{ 1.00,-2.00},{ 1.00,-1.50},{ 1.00,-1.00},              { 1.00,-0.50},              { 1.00, 0.00},              { 1.00, 0.50},              { 1.00, 1.00},{ 1.00, 1.50},{ 1.00, 2.00},
			                { 1.50,-1.00},              { 1.50,-0.50},              { 1.50, 0.00},              { 1.50, 0.50},              { 1.50, 1.00},
		                                                                            { 2.00, 0.00},
		                    { 0.0, 0.0} };  // in piu', per arrivare a 64
// Cosa avevo in testa, quando ho disegnato questo abominio?
*/
/*
__device__ __constant__ MV_ref MV_lookup_refin_tex[] = { 	// 81 candidati
	{-1.00,-1.00},{-1.00,-0.75},{-1.00,-0.50},{-1.00,-0.25},{-1.00, 0.00},{-1.00, 0.25},{-1.00, 0.50},{-1.00, 0.75},{-1.00, 1.00},
	{-0.75,-1.00},{-0.75,-0.75},{-0.75,-0.50},{-0.75,-0.25},{-0.75, 0.00},{-0.75, 0.25},{-0.75, 0.50},{-0.75, 0.75},{-0.75, 1.00},
	{-0.50,-1.00},{-0.50,-0.75},{-0.50,-0.50},{-0.50,-0.25},{-0.50, 0.00},{-0.50, 0.25},{-0.50, 0.50},{-0.50, 0.75},{-0.50, 1.00},
	{-0.25,-1.00},{-0.25,-0.75},{-0.25,-0.50},{-0.25,-0.25},{-0.25, 0.00},{-0.25, 0.25},{-0.25, 0.50},{-0.25, 0.75},{-0.25, 1.00},
	{ 0.00,-1.00},{ 0.00,-0.75},{ 0.00,-0.50},{ 0.00,-0.25},{ 0.00, 0.00},{ 0.00, 0.25},{ 0.00, 0.50},{ 0.00, 0.75},{ 0.00, 1.00},
	{ 0.25,-1.00},{ 0.25,-0.75},{ 0.25,-0.50},{ 0.25,-0.25},{ 0.25, 0.00},{ 0.25, 0.25},{ 0.25, 0.50},{ 0.25, 0.75},{ 0.25, 1.00},
	{ 0.50,-1.00},{ 0.50,-0.75},{ 0.50,-0.50},{ 0.50,-0.25},{ 0.50, 0.00},{ 0.50, 0.25},{ 0.50, 0.50},{ 0.50, 0.75},{ 0.50, 1.00},
	{ 0.75,-1.00},{ 0.75,-0.75},{ 0.75,-0.50},{ 0.75,-0.25},{ 0.75, 0.00},{ 0.75, 0.25},{ 0.75, 0.50},{ 0.75, 0.75},{ 0.75, 1.00},
	{ 1.00,-1.00},{ 1.00,-0.75},{ 1.00,-0.50},{ 1.00,-0.25},{ 1.00, 0.00},{ 1.00, 0.25},{ 1.00, 0.50},{ 1.00, 0.75},{ 1.00, 1.00}
};
*/


#ifdef __cplusplus
}
#endif


#endif //_ME_DIAMONDS
