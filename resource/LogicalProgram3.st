PROGRAM DevelopmentRulesTest
VAR
    Values      : ARRAY [0..2] OF INT;

    A           : INT := 3;
    B           : INT := 2;
    R           : REAL := 0.0;
    R2          : REAL := 1.0;

    NotInit     : INT;
    Result      : INT := 0;
    Index       : INT := 22;
END_VAR

(* OK: index is inside declared array bounds. *)
Values[1] := 10;

(* ERROR: explicit constant index is outside ARRAY [0..2]. *)
Values[3] := 20;

(* OK: variable index is not checked by this rule yet. *)
Values[Index] := 30;

(* ERROR: direct REAL comparison through '='. *)
IF R = R2 THEN
    Result := Result + 1;
END_IF;

(* ERROR: direct REAL comparison through '<>'. *)
IF R <> R2 THEN
    Result := Result + 1;
END_IF;

(* ERROR: NotInit is declared, but no value was assigned before this use. *)
Result := NotInit + 1;

(* OK: NotInit is assigned before the next use. *)
NotInit := 7;
Result := NotInit + 1;

(* ERROR: integer division happens first, then the truncated result goes to REAL. *)
R := A / B;

(* OK: at least one operand of the division is explicitly REAL. *)
R := A / 2.0;

END_PROGRAM
