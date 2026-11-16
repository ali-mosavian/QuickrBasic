DECLARE FUNCTION f(n)
PRINT f(23)
FUNCTION f(n)
IF n < 2 THEN f = n ELSE f = f(n - 1) + f(n - 2)
END FUNCTION
