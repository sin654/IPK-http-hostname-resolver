#Dokumentace k projektu IPK 2020: HTTP resolver doménových jmen
Cílem projektu je implementace severu, který bude komunikovat protokolem HTTP a bude zajišťovat překlad doménových jmen. Pro překlad jmen bude server používat lokální resolver stanice na které běží.

#Překlad a spuštění
Projekt je implementován v jazyce C. Zip archiv obsahuje Makefile, tuto dokumentaci a ve složce /src zdrojový soubor projektu.
make build //přeloží server
make run PORT=1234 //spustí server na portu 1234

#Popis implementace
Server po úspěšném zpracování argumentů získá informace pomocí funkce **getaddrinfo()**, poté vytvoří socket funkcí **socket()**, tento socket je poté navázán na port pomocí funkce **bind()**. Server začně naslouchat na tomto portu pomocí funkce **listen()**. Po této inicializaci služeb začně nekonečný cyklus serveru, ve kterém je pomocí funkce **accept()** navázáno nové spojení a následně jsou funkcí **recv()** přijaty data HTTP dotazu od klienta.

Tento HTTP dotaz je serverem zpracován a následně server vytvoří a odešle klientovi adekvátní odpověď pomocí funkce **send()**.

#Typy odpovědí od serveru
Pokud má HTTP dotaz správný formát a je nalezen překlad IP/doménového jména, tak jako odpověď přijde HTTP odpověd s hlavičkou **HTTP/1.1 200 OK** a daty ve formátu podle zadání tohoto projektu v těle odpovědi. (hlavička obsahuje i další informace, jako například délka dat, typ dat ...). Data v těle odpovědi mají formát **DOTAZ:TYP=ODPOVED** například **apple.com:A=17.142.160.59**, kde typ může být **A** -> překlad z doménového jména na IP adresy, nebo **PTR** -> překlad z IP adresy na doménové jméno.

Dalšími možnýmy odpověďmi (chybové) jsou HTTP odpovědi s hlavičkou **HTTP/1.1 404 Not Found**, **HTTP/1.1 400 Bad Request**, **HTTP/1.1 405 Method Not Allowed**, **HTTP/1.1 500 Internal Server Error**. Tyto chybové odpovědi mají navíc i jako tělo uvedeno číslo chyby a její popis stejně jako v hlavičce např. **500 Internal Server Error** atd. pro vyhodnoceni odpovědi pomocí nástroje **curl**, který by jinak nic nevypsal, kdyby v těle odpovědi nic nebylo.

#Důležité !!!!!!!!!!! pro vyhodnocení vstupů
Protože zadání nespecifikuje všechny možnosti a ani po odpovědích na fóru nebylo možné přesně určit, jak by se program měl zachovat, tak zde doplním několik informací.
Pokud bude v **POST** požadavku nějaký záznam neplatný ( špatný formát, pokus o překládání doménového jména s typem PTR...) bude odpovědí 400 Bad Request. Naopak pokud toto nenastane a pouze nebude možné nalézt odpověď, tak tento záznam bude pouze vynechán.
