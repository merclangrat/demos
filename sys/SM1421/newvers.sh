MSG=l
export MSG
if [ ! -r version ]; then echo 0 > version; fi
touch version
awk '   {       version = $1 + 1; }\
END     {       printf "char version[] = \"DEMOS 2.2 (SM1421.%d) co-operative demos/* ", version > "vers.c";\
		printf "%d\n", version > "tmpvers"; }' < version
mv tmpvers version
echo `date`'\n";' >> vers.c
