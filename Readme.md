# φLISP

オレオレ LISP を作ってみたかった

## 目標

コンパクトでいて高い表現力、(多少安全性を犠牲にしてでも) 書いていて楽し
い高い自由度

## 特徴

* LISP 系

* 1-lisp (関数と変数の名前空間は共通)

* 動的束縛が基本、関数閉包は明示的に作る

* 引数を評価するかを関数側で指定できる

* 後置記法、中置記法が部分的に使える

* 関数を作らずに部分適用ができる

* 末尾呼び最適化

* 第一級継続

* Pure C 実装

* ...

## コンパイル

```text
gcc *.c
```

## 基本データ

φLISP の基本データは

* * 文字
* * 整数
* * 浮動小数点数
* * 配列
* リスト・ペア
* シンボル
* 関数・関数閉包
* 継続
* ストリーム (FILE* に相当)

です。まずは上の４つをざっくり見てみます。

文字は `?` で表現します。C と同様のエスケープシーケンスを書くことができ
ます。

```text
>> ?a
?a

>> ?\n
?\n
```

整数、浮動小数点数の表現は C とだいたい同じです。

```text
>> 10
10

>> 10.
10.000000

>> .1
1.000000

>> 1e5
100000
```

配列は `[]` で表現します。

```text
>> [1 2 3]
[1 2 3]
```

文字列は (ユーザーから見ると) 要素がすべて文字の配列です。

```text
>> "hoge"
"hoge"

>> [?h ?o ?g ?e]
"hoge"
```

-----

内部的には、文字列と配列は区別して扱われています。なので、例えば文字列
(文字の配列) の第１要素に整数の 1 を代入する、のような文字列と配列を行
き来するような操作を激しく行うプログラムは、実行はできますが書かない方
がいいです。

## 関数呼び出しとリスト

関数呼び出しは、一般的な LISP と同様に、関数と引数を並べて括弧でくくる
ことで表現します。

```text
(関数 引数 引数 ...)
```

たとえば、式 `(mod 5 3)` を評価すると `2` が得られます。

```text
>> (mod 5 3)
2
```

一方、 φLISP のもっとも基本的なデータ構造は、これも一般的な LISP と同
様にリストです。リストは、要素を括弧でくくることで表現します。

```text
(要素 要素 ...)
```

これでは関数呼び出しとリストの見分けがつかなくなってしまうのですが、こ
れが LISP 系言語の強みでもあります。関数呼び出しを表す式 `(mod 5 3)` は、
実は `mod` と `5` と `3` のリストです。このリストを「評価すると」、値
2 が得られます。評価せずにたんにリストを作りたい場合には、リストの前に
クォートを付けます。

```text
>> '(mod 5 3)
(mod 5 3)
```

たとえばこのリストを、リストの先頭要素を返す関数 `car` に渡すと `mod`
が得られます。

```text
>> (car '(mod 5 3))
mod
```

## cons と ()

φLISP では、２つのもののペア (ドット対) を次のようにドットで表現します。

```text
>> '(1 . 2)
(1 . 2)
```

あるいは、２つのものをペアにする関数 `cons` を使って表現してもかまいま
せん。

```text
>> (cons 1 2)
(1 . 2)
```

一方、 `()` は見た目通り、空リストを表すオブジェクトです。

一般的な LISP と同様に、すべての (空でない) リストは、実は `()` と
=cons= から作られています。たとえば、リスト `(1 2 3)` は、実は `(1
. (2 . (3 . ())))` の略記です。

```text
>> '(1 . (2 . (3 . ())))
(1 2 3)
```

したがって、 `cons` はリストに要素を１つ追加する関数にもなっています。

```text
>> (cons 1 '(2 3))
(1 2 3)
```

ドット対の第一要素を CAR 部、第二要素を CDR 部といい、ドット対からこれ
らを取り出す関数 `car`, `cdr` が用意されています。

```text
>> (car '(1 . 2))
1

>> (cdr '(1 . 2))
2
```

リストはただのドット対だったので、 `car`, `cdr` はリストに対しても使え
ます。このとき、ちょうど `car` はリストの先頭要素を取り出す関数、
`cdr` はリストから先頭要素を除いた残りを取り出す関数として機能します。

```text
>> (car '(1 2 3))
1

>> (cdr '(1 2 3))
(2 3)
```

末尾が `()` で終端されていないリストというのを考えることもできます (非
真性リストとかいうことがあります)。このようなリストを書くために、次の
ような略記法が用意されています。

```text
>> '(1 . (2 . (3 . 4)))
(1 2 3 . 4)

>> '(1 2 3 . 4)
(1 2 3 . 4)
```

-----

ペアから要素を取り出す関数の名前 `car`, `cdr` は歴史的にそうなっている
のだそうで、 `head`, `tail` のほうがいいという人もよくいます。が、やっ
ぱり `car`, `cdr` が僕にはしっくりきました。

## シンボル

一般的な言語で "変数名" や "関数名" に相当するのがシンボルです。たとえ
ばリストの例で出てきた `mod` はシンボルです。

```text
>> (car '(mod 5 3))
mod
```

シンボルは、評価するとそのシンボルの指す値が得られるオブジェクトです。
試しに `mod` を評価してみると、組込み関数 mod のオブジェクトが得られま
す。

```text
>> mod
#<subr:2 subr_mod>
```

シンボルもリストと同様に、クォートを付けることで "シンボル自体" を表現
することができます。

```text
>> 'mod
mod
```

シンボルを値に束縛する (≒変数に値を代入する) ために、関数 `bind!` を用
います。たとえばシンボル `hoge` を値 `1` に束縛するには、 `(bind!
'hoge 1)` を評価します。

```text
>> (bind! 'hoge 1)
1
```

いま、 `hoge` を評価すれば `1` が得られ、 `(+ hoge 2)` を評価すれば
`3` が得られます。

```text
>> hoge
1

>> (+ hoge 2)
3
```

シンボルもただのデータなので、シンボルに束縛することができます。

```text
>> (bind! 'hoge 'fuga)
fuga

>> hoge
fuga
```

見かけによらず、 φLISP のシンボルは名前を持っていません。代わりに、シ
ンボルテーブルと呼ばれる、名前からシンボルへの単射があります (すべての
名前には対応するシンボルがあるが、すべてのシンボルに対応する名前がある
とは限らない)。一般的な LISP にもシンボルテーブルはありますが、ふつう、
シンボルのオブジェクトもまた名前を保持しています。

名前からシンボルを取得するには関数 `intern` を、どの名前にも割り当てら
れていないシンボルを１つ取得するには関数 `gensym` を使います。

```text
>> (intern "hoge")
hoge

>> (gensym)
#<symbol 0x600083fc0>
```

古い LISP とは異なり、 `(intern "hoge")` と `(intern "HOGE")` は別のシ
ンボルを返します。

-----

変数を束縛する関数の名前は `set!` と `bind!` で迷っています。どうしよう
かな。

## eval

(クォートされた) データを評価したくなったら、データの前にカンマを付けま
す。

```text
>> '(mod 5 3)
(mod 5 3)

>> ,'(mod 5 3)
2
```

```text
>> (bind! 'foo 1)
1

>> (bind! 'bar 'foo)
foo

>> (bind! 'baz 'bar)
bar

>> 'baz
baz

>> baz
bar

>> ,baz
foo

>> ,,baz
1
```

リストやシンボル以外は、何度評価しても自分自身に評価されます
(self-evaluating な値とかいいます)。

```text
>> ,1
1
```

## 関数

一般的な LISP と同様に、 φLISP では関数は第一級のオブジェクトです。関
数を作るために、組み込み関数 `fn` を使います。

```text
>> (fn (x y) (+ x y))
#<func:2 (+ ...)>
```

この関数は、２つの引数 `x`, `y` を受け取って、その和を返す関数です。ためし
に `2`, `3` を渡してみます。

```text
>> ((fn (x y) (+ x y)) 2 3)
5
```

関数も他の値と同じようにシンボルに束縛できます。

```text
>> (bind! 'add (fn (x y) (+ x y)))
#<func:2 (+ ...)>

>> (add 1 2)
3
```

不特定多数の引数を受け取る関数を、関数の引数リストに非真性リストを書く
ことで表現できます。

```text
>> (fn (x . y) x)
#<func:1+ 0x600081ee0>

>> ((fn (x . y) x) 1 2 3 4)
1

>> ((fn (x . y) y) 1 2 3 4)
(2 3 4)
```

-----

関数を作るマクロ (φLISP では関数) の名前 `fn` は UnCL を参考にしました
(どうでもいいけど、Scheme も元々は "an uncommon lisp" を名乗っていまし
たね)。 `lambda` と迷いましたが、よく書くのでやっぱり短い方がいいですね。

可変長引数の構文は Scheme から借りています。とてもきれいな構文で気に入っ
ています (が、オプショナル引数が書けないのは少し不便かも)。

## 名前受け取りをする関数

φLISP では、引数を評価するか否かを関数側から指定することができます。
引数リストの中で、評価せずに受け取りたい引数にカンマを付けます。

```text
>> ((fn (x) x) car)
#<subr:1 subr_car>

>> ((fn (,x) x) car)
car
```

これを使って、たとえば次のような関数を定義することができます。

```text
>> (bind! 'define (fn (,var val) (bind! var val)))
#<func:2 (bind! ...)>

>> (define fuga 1)
1

>> fuga
1
```

`(define a b)` はちょうど `(bind! 'a b)` と等価になっています。

-----

φLISP には、名前受け取り関数がある代わりに、マクロはありません。古い
LISP の fexpr に近いかな？

名前受け取り関数はマクロではないので、一般的な LISP とは異なり、
`apply` できたりします。

```text
(apply define '(a 1))
```

関数の中で名前受け取り関数が呼ばれたときの挙動が、わかりにくいし使いづ
らいので、現在の挙動は暫定です。

```text
>> (bind! 'map (fn (f l) (if l (cons (f (car l)) (map f (cdr l))) ())))
#<func:2 (if ...)>

>> (map quote '(1 2 3))
((car l) (car l) (car l))
```

## 条件分岐

組込み関数 `if` は３つの引数を取り、そのうち後ろの２つは名前受け取りを
します。

```text
(fn (a ,b ,c) ...)
```

もし第一引数の評価値が `()` であれば式 `c` を、そうでなければ式 `b` を
評価し、得られた値を返します。

```text
>> (if () 1 2)
2

>> (if 'a 1 2)
1
```

-----

`#f` (false を表す特別な値) 以外はすべて真 (`()` も真) 、とする LISP も
ありますが、リスト処理などでなんだかんだ便利なので `()` を偽にしました。

## 変数束縛と関数閉包

φLISP の変数はデフォルトで動的に束縛されます。たとえば

```text
>> (bind! 'x 1)
1

>> (bind! 'f (fn (y) (+ x y)))
#<func:1 (+ ...)>
```

という関数を定義したとき、

```text
>> ((fn (x) (f 1)) 10)
11
```

になります (束縛が静的であれば、この式の評価値は `2` です)。

動的束縛なので、不動点演算子などを用いなくても再帰関数が書けます。

```text
>> (bind! 'fact (fn (n) (if (= n 0) 1 (* n (fact (- n 1))))))
#<func:1 (if ...)>

>> (fact 5)
120
```

変数が静的に束縛された関数 (関数閉包) を作るためには、関数を `closure`
に渡します。

```text
>> (bind! 'x 1)
1

>> (bind! 'f (closure (fn (y) (+ x y))))
#<closure:1 0x600081930>

>> ((fn (x) (f 1)) 10)
2
```

関数閉包を利用すると、たとえば状態を持った関数を作ることができます。

```text
>> ((fn (x) (bind! 'count (closure (fn () (bind! 'x (+ x 1)))))) 0)
#<closure:0 0x600087b70>

>> (count)
1

>> (count)
2

>> (count)
3

>> (bind! 'x 10)
10

>> (count)
4
```

デフォルトで動的束縛であることによって、すでに定義されている関数の挙動
を後から変えたり、あるいは再定義したりすることが簡単にできます。もちろ
んうかつに使うとバグのもとになる危険な機能ですが、ここでは安全性よりも
自由度、楽しさを優先しました。

たとえば、式を評価する関数 `eval` を、何もしない関数で上書きすると、

```text
>> (bind! 'eval (fn (x) x))
#<func:1 0x600082120>

>> a
a

>> (+ 1 2)
(+ 1 2)
```

もはや式は評価されなくなります。

同様に、もしも静的束縛な言語が好きならいつでも関数閉包を作るように
`fn` を再定義ることができるし、もしも `()` が偽になるのが気に食わなけれ
ば `if` を再定義することができます。

## 制御構造 / 第一級継続

`if` 以外の制御構造は `call-cc` だけです。だけですが、こいつは大域脱出
からコルーチンまでこなすすごい奴です。 C の `longjmp` の上位互換といえ
ばすごさが伝わるかもしれません。僕にはうまく説明できないので、詳しくは
他の資料を探してください。

```text
>> (call-cc (fn (cc) (+ 1 (cc 2))))
2
```

-----

`unwind-protect` は未実装ですが、いずれ実装します。

動的に束縛された変数があるときの CPS 変換がちょっと怖かったので、CPS変
換ではなくコールスタックを自前で管理する方法で実装しました。C の関数呼
び出しのしくみを使えないので、 `eval` が `goto` まみれになって大変でし
た。

## ファイル IO

省略。 Scheme のポートっぽい感じのことができ〼。

## 共有オブジェクトのロード

DLL を動的にロードして φLISP の関数として呼び出すことができます。

## 例外の扱い

制御構造は `call-cc` だけなので、例外処理のしくみは原則ありません。かわ
りに、失敗する可能性のある関数はエラー時に呼び出されるオブジェクトを
(任意に) 受け取ります。

たとえば、シンボルの束縛されている値を返す関数 `bound-value` は、シン
ボルが未束縛であればエラーになります。

```text
>> (bound-value 'hoge)
ERROR: reference to unbound symbol.
```

が、 `bound-value` は２つ目の引数として、エラー時に呼び出される関数を与
えることができます。シンボルが未束縛のとき、失敗時の関数が与えられてい
るならば、この関数をエラーメッセージを引数として呼び出した結果が全体の
結果になります (引数の仕様は暫定)。

```text
>> (bound-value 'hoge (fn (x) x))
"reference to unbound symbol"
```

たとえばエラー時の関数として `(fn (x) ())` を与えれば、「失敗時には
`()` を返す」という挙動を、

```text
>> (bound-value 'hoge (fn (x) ()))
()
```

あるいは継続を与えれば例外処理のような大域脱出を表現できます。

```text
>> (call-cc (fn (cc) (+ 1 (bound-value 'hoge cc))))
"reference to unbound symbol."
```

## 末尾呼びの最適化

φLISP の処理系は真性に末尾再帰的です。すなわち、関数呼び出しを行うとき、
本当に必要がある場合にだけスタックを消費します。

たとえば次のコードは (GC の実装が終われば) 、スタックオーバーフローせず
に無限ループします (してほしい)。

```text
>> (bind! 'loop (fn () (loop)))
#<func:0 (loop ...)>

>> (loop)
```

加えて、eval(`,`) 内の式の末尾位置での呼び出しも最適化されます。これは
インタプリタならではな気がします。たぶん。

```text
>> (bind! 'loop (fn () ,'(loop)))
#<func:0 (eval ...)>
```

-----

動的束縛では末尾呼びの最適化は難しいんじゃないかと一瞬思ったのですが、
どうやらできるらしいことが LISP の古文書に載っていました
( http://ci.nii.ac.jp/naid/110002720392 )。

## Codez comme vous voulez

楽しい、自由度の高い言語にするために、 φLISP ではほかの LISP では認め
ていないような書き方をいくつか認めています。

### 関数オブジェクトを作らない部分適用

関数適用において、引数の数が足りないとき、一般的な LISP ではエラーにな
ります。 φLISP では、これを部分適用として扱います。

たとえば、 `(cons 1)` は `(fn (x) (cons 1 x))` と等価です。

```text
>> (cons 1)
#<func:(pa/#<subr:2 subr_cons>)>

>> ((cons 1) 2)
(1 . 2)
```

-----

内部的には、部分適用は関数オブジェクトとは別の特別なオブジェクトで管理
されているので、 `(cons 1)` はむしろ `(fn (x) (cons 1 x))` よりも効率が
いいです。

### 中置記法

たとえば `(f 1)` をうっかり `(1 f)` と書いてしまうことはまずありません。
それならば、 `(1 f)` をエラーとするのではなく、この記法にもなにか意味を
与えたらどうか、というのが基本的なアイデアです。

φLISP では、整数・浮動小数点数に対する関数呼び出しを、次のルールで評価
します。

```text
(1 f x ...) = ((f 1 x) ...)
(1 f)       = (fn (y) (f 1 y))
(1)         = 1
```

たとえば、

```text
(1 + 2 + 3) = ((+ 1 2) + 3) = (3 + 3) = ((+ 3 3)) = (6) = 6
```

という具合です。これによって、 LISP の苦手な (?) 数式を、中置記法で書く
ことができます (そうするかどうかは自由です)。ただし、すべての演算子の優
先度が同じかつ左結合であることに注意してください。

２番目のルールは、数値の後に関数が１つだけ続いた場合には、これを後置記
法ではなく中置記法の部分適用として扱うことを表しています。この性質を使
うと、たとえば次のようなコード

```text
(filter (fn (x) (< 0 x)) '(-1 0 1 2)) => '(1 2)
```

を

```text
(filter (0 <) '(-1 0 1 2))
```

と書くことができます。

### 後置記法

φLISP では、関数でも整数・浮動小数点数でもないオブジェクトの関数呼び出
しを、次のようなルールで評価します。

```text
('(1 2 3) f ...) = ((f '(1 2 3)) ...)
('(1 2 3))       = '(1 2 3)
```

たとえば、

```text
('(1 2 3) cdr car) = ((cdr '(1 2 3)) car) = ('(2 3) car) = ((car '(2 3))) = (2) = 2
```

となります。これによって、メソッドチェインのような書き方ができます。

```text
("hoge fuga piyo" (split-string ?\s) car) => "hoge"
```

### 述語はなるべく意味のある値を返す

引数を受け取って true または false (LISP 的には、 `()` かそれ以外) を返
す組込み関数は、なるべく意味の値を返すようになっています。たとえば、
`(< 1 2)` (中置記法を使えば、 `(1 < 2)`) の評価値は `2` です。この性質
は次のように利用できます。

```text
>> (1 < 2 < 3)
3

>> (1 < 2 < 2)
()
```

*** シンボル以外のオブジェクトの束縛

一般的な LISP と異なり、 φLISP ではシンボル以外のオブジェクトも値に束
縛することができます。束縛された値は関数 `bound-value` で取り出すことが
できます。

```text
>> (bind! 'a 1)
1

>> (bind! a 2)
2

>> (bound-value 'a)
1

>> (bound-value a)
2
```

何に使うかはまだよく考えていませんが、いつか何かの役に立つかもしれませ
ん。

## 組込み関数の全一覧

ソースコードからの抽出です。読みにくくてごめんなさい。

(nil? O) => an unspecified non-() value if O is (), or () otherwise.

(symbol? O) => O if O is a symbol, or () otherwise.

(gensym) => an uninterned symbol.

(intern NAME) => a symbol associated with NAME.

(bind! O1 [O2]) => bind O1 to object O2 in the innermost scope and
return O2. if O2 is omitted, bind O1 to ().

(bound-value O [ERRORBACK]) => object which O is bound to. if O is
unbound, call ERRORBACK with error message, or error if ERRORBACK is
omitted.

(character? O) => O if O is a character, or () otherwise.

(char->int CHAR) => ASCII encode CHAR.

(int->char N) => ASCII decode N.

(integer? O) => O if O is an integer, or () otherwise.

(float? O) => O if O is a float, or () otherwise.

(mod INT1 INT2) => return (INT1 % INT2).

(/ INT1 INT2 ...) => return (INT1 / INT2 / ...).

(round NUM) => the largest integer no greater than NUM.

(+ NUM1 ...) => sum of NUM1, NUM2, ... . result is an integer iff
NUM1, NUM2, ... are all integer.

(* NUM1 ...) => product of NUM1, NUM2, ... . result is an integer iff
NUM1, NUM2, ... are all integer.

(- NUM1 NUM2 ...) => negate NUM1 or subtract NUM2 ... from
NUM1. result is an integer iff NUM1, NUM2 ... are all integers.

(div NUM1 NUM2 ...) => invert NUM1 or divide NUM1 with NUM2
... . result is always a float.

(<= NUM1 ...) => last number if NUM1 ... is weakly increasing, or ()
otherwise. if no numbers are given, return an unspecified non-()
value.

(< NUM1 ...) => last number if NUM1 ... is strongly increasing, or ()
otherwise. if no numbers are given, return an unspecified non-()
value.

(>= NUM1 ...) => last number if NUM1 ... is weakly decreasing, or ()
otherwise. if no numbers are given, return an unspecified non-()
value.

(> NUM1 ...) => last number if NUM1 ... is strongly decreasing, or ()
otherwise. if no numbers are given, return an unspecified non-()
value.

(stream? O) => O if O is a stream, or () otherwise.

(input-port) => current input port, which defaults to stdin.

(output-port) => current output port, which defaults to stdout.

(error-port) => current error port, which defaults to stderr.

(set-ports [ISTREAM OSTREAM ESTREAM]) => change input port to ISTREAM
(resp. output port, error port). some of arguments can be omitted or
(), which represents "no-change". (return value is unspecified)

(getc [ERRORBACK]) => get a character from input port. on failure,
ERRORBACK is called with error message, or error if ERRORBACK is
omitted.

(putc CHAR [ERRORBACK]) => write CHAR to output port and return
CHAR. on failure, ERRORBACK is called with error message, or error if
ERRORBACK is omitted.

(puts STRING [ERRORBACK]) => write STRING to output port and return
STRING. on failure, ERRORBACK is called with error message, or error
if ERRORBACK is omitted.

(ungetc CHAR [ERRORBACK]) => unget CHAR from input stream and return
CHAR. when this subr is called multiple times without re-getting the
ungot char, behavior is not guaranteed. on failure, ERRORBACK is
called with error message, or error if ERRORBACK is omitted.

(open FILE [WRITABLE APPEND BINARY ERRORBACK]) => open a stream for
FILE. if WRITABLE is omitted or (), open FILE in read-only
mode. (resp. APPEND, BINARY)

(close! STREAM [ERRORBACK]) => close STREAM (return value is
unspecified). on failure, ERRORBACK is called with error message, or
error if ERRORBACK is omitted.

(cons? O) => O if O is a pair, or () otherwise.

(cons O1 O2) => pair of O1 and O2.

(car PAIR) => CAR part of PAIR. if PAIR is (), return ().

(cdr PAIR) => CDR part of PAIR. if PAIR is (), return ().

(setcar! PAIR NEWCAR) => set CAR part of PAIR to NEWCAR. return NEWCAR.

(setcdr! PAIR NEWCDR) => set CDR part of PAIR to NEWCDR. return NEWCDR.

(array? O) => O if O is an array, or () otherwise.

(make-array LENGTH [INIT]) => make an array of LENGTH slots which
defaults to INIT. if INIT is omitted, initialize with () instead.

(aref ARRAY N) => N-th element of ARRAY. error if N is negative or
greater than the length of ARRAY.

(aset! ARRAY N O) => set N-th element of ARRAY to O and return
O. error if O is negative or greater than the length of ARRAY.

(string? O) => O if O is a char-array, or () otherwise.

(function? O) => O iff O is a function, or () otherwise.

(fn ,FORMALS ,EXPR) => a function.

(closure? O) => O iff O is a function, or () otherwise.

(closure FN) => make a closure of function FN.

(subr? O) => O iff O is a subr (a compiled function), or () otherwise.

(dlsubr FILENAME SUBRNAME [ERRORBACK]) => load SUBRNAME from
FILENAME. on failure, ERRORBACK is called with error message, or error
if ERRORBACK is omitted.

(continuation? O) => O iff O is a continuation object, or ()
otherwise.

(eq O1 ...) => an unspecified non-() value if O1 ... are all the same
object, or () otherwise.

(char= CH1 ...) => last char if CH1 ... are all equal as chars, or ()
otherwise. if no characters are given, return an unspecified non-()
value.

(= NUM1 ...) => last number if NUM1 ... are all equal as numbers, or
() otherwise. if no numbers are given, return an unspecified non-()
value.

(print O) => print string representation of object O to output port
and return O.

(read [ERRORBACK]) => read an S-expression from input port. on
failure, ERRORBACK is called with error message, or error if ERRORBACK
is omitted.

(if COND ,THEN [,ELSE]) => if COND is non-(), evaluate THEN, else
evaluate ELSE. if ELSE is omitted, return ().

(evlis PROC EXPRS) => evaluate list of expressions in accordance with
evaluation rule of PROC.

(apply PROC ARGS) => apply ARGS to PROC.

(unwind-protect ,BODY ,AFTER) => evaluate BODY and then AFTER. when a
continuation is called in BODY, evaluate AFTER before winding the
continuation.

(call-cc FUNC) => evaluate BODY and then AFTER. when a continuation is
called in BODY, evaluate AFTER before winding the continuation.

(eval O [ERRORBACK]) => evaluate O. on failure, call ERRORBACK with
error message, or error if ERRORBACK is omitted.

(quote ,O) => O.

(error MSG) => print MSG to error port and quit.

## 今後やること

今の時点では完全にオモチャ処理系です。いろいろ直さねば…。

### バグ

* REPL がハリボテなので束縛の管理が信用できない (末尾呼び最適化のせい)

```text
>> (bind! 'x 1)
1

>> ((fn (x) x) 2)
2

>> x
2
```

!?!?

* 保存した継続を起動すると引数が多すぎると言われる
  * 戻りがけで継続を破壊的に変更してるから

* 名前呼びなのに無限ループ？

```text
(bind! 'Y (fn (f) (f (Y f))))
((Y (fn (,f n) (if (n = 0) 1 (n * (,f (n - 1)))))) 1)
```

* int や float もすべてオブジェクトなので効率が悪い

### 未実装

* evlis, unwind-protect

* GC

* コメント

## 影響を受けた言語

* Emacs Lisp
* Scheme
* TAO
* CommonLisp
* Smalltalk
* Lisp 1.5
* NewLisp
* UnCL
* Haskell
