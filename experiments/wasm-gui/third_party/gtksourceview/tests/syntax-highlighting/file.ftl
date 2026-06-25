# Comment

## Multiline

multi = Text can also span multiple lines
    as long as each new line is indented
    by at least one space.
single = This is a single line message

## Terms

-brand-name = Gnome
installing = Installing { -brand-name }

## Special characters

spaces = {"    "}This message starts with 3 spaces
tears-of-joy = Use {"\U01F602"} or 😂

## Varibles

unread-emails = { $user } has { $email-count } unread emails

## Selectors & Functions

your-rank = { NUMBER($pos, type: "ordinal") ->
   [1] You finished first!
   [one] You finished {$pos}st
   [two] You finished { $pos }nd
   [few] You finished {$pos}rd
  *[other] You finished {$pos}th
}

## Attributes

login-input = Predefined value
    .placeholder = email@example.com
    .aria-label = Login input value
    .title = Type your login email

