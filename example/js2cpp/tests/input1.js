{
    var temporary = 0;
    
    var bar = function (object)
    {
        return 10;
    };
    
    var foo = function ()
    {
        var object;
        var result = function() { return object; };
        return function(argument) { return bar( object ); };
    };
    
    extern "for (int i = 0; i < 1000000; ++ i)";
        (foo())(temporary);
}
