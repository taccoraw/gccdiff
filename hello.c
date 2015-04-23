#define MAX 12
#define ADD(a, b) ((a) + (b))
#define CALC(x) ADD(x, MAX)
int main()
{
	int a = 1;
	return CALC(a);
}
