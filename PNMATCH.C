#include <stdlib.h>

/* File name pattern matching function */

int	pnmatch (string, pattern, flag)
char	*string;			/* String to match                  */
char	*pattern;			/* Pattern to match against         */
int	flag;				/* Match using '$' & '^'            */
{
    register int	cur_s;		/* Current string character         */
    register int	cur_p;		/* Current pattern character        */

/* Match $ and ^ ? */

    if (flag == 1)
    {
	while (*string)
	{
	    if (pnmatch (string++, pattern, ++flag))
		return 1;
	}

	return 0;
    }

/* Match string */

    while (cur_p = *(pattern++))
    {
	cur_s = *(string++);		/* Load current string character    */

        switch (cur_p)			/* Switch on pattern character      */
        {
            case '^':			/* Match start of string            */
            {
                if (flag == 2)
                    string--;

                else if ((flag) || (cur_p != cur_s))
		    return 0;

                break;
            }

            case '$':			/* Match end of string              */
            {
                if (!flag)
                {
                    if (cur_p != cur_s)
			return 0;

                    break;
                }

                else
		    return ((cur_s) ? 0 : 1);
            }

            case '[':			/* Match class of characters        */
            {
                while(1)
                {
                    if (!(cur_p = *(pattern++)))
			return 0;

                    if (cur_p == ']')
			return 0;

                    if (cur_s != cur_p)
                    {
                        if (*pattern == '-')
                        {
                            if(cur_p > cur_s)
                                continue;

                            if (cur_s > *(++pattern))
                                continue;
                        }
                        else
                            continue;
                    }

                    break;
                }

                while (*pattern)
                {
                    if (*(pattern++) == ']')
                        break;
                }
            }

            case '?':			/* Match any character              */
            {
                if (!cur_s)
		    return 0;

                break;
            }

            case '*':			/* Match any number of any character*/
            {
                string--;

                do
                {
                    if (pnmatch (string, pattern, 0))
			return 1;
                }
                while (*(string++));

		return 0;
            }

            case '\\':			/* Next character is non-meta       */
            {
                if (!(cur_p = *(pattern++)))
		    return 0;
            }

            default:			/* Match against current pattern    */
            {
                if (cur_p != cur_s)
		    return 0;

                break;
            }
        }
    }

    return ((flag || (!(*string))) ? 1 : 0);
}
