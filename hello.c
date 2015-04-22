#define MAX 10
int fun1();
int c=1;
int main()
{
	fun1(1,2);
	return 1;
}
void fun(int a , int b)
{
	if (a==0)
		a=(a+b)*c;
	else
		b=1;

}
int fun1(const int x,int*** y)
{
	int a=0;int b=1;
	c=b;
	fun(0,1);
	if(a==0)
		return a+b;
	else return MAX;
}
long fun2(const char* zz) { return (long)zz; }
