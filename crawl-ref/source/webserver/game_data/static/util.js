define([],
function () {
    var cols = {
        "black": 0,
        "blue": 1,
        "green": 2,
        "cyan": 3,
        "red": 4,
        "magenta": 5,
        "brown": 6,
        "lightgrey": 7,
        "lightgray": 7,
        "darkgrey": 8,
        "darkgray": 8,
        "lightblue": 9,
        "lightgreen": 10,
        "lightcyan": 11,
        "lightred": 12,
        "lightmagenta": 13,
        "yellow": 14,
        "white": 15
    };

    function formatted_string_to_html(str)
    {
        var other_open = false;
        return str.replace(/<(\/?[a-z]+)>/ig, function (str, p1) {
            var closing = false;
            if (p1.match(/^\//))
            {
                p1 = p1.substr(1);
                closing = true;
            }
            if (p1 in cols)
            {
                if (closing)
                    return "</span>";
                else
                {
                    var text = "<span class='fg" + cols[p1] + "'>";
                    if (other_open)
                        text = "</span>" + text;
                    other_open = true;
                    return text;
                }
            }
            else
                return str;
        }).replace(/<</g, "<");
    }

    return {
        formatted_string_to_html: formatted_string_to_html
    };
});
