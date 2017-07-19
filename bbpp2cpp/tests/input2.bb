class Document
{
    var head = new int(0);
    var tail = new int(0);

    Document() 
    {
        // cycle
        head = tail; 
        tail = head; 
        cout << __PRETTY_FUNCTION__ << endl; 
    }
    ~Document() { cout << __PRETTY_FUNCTION__ << endl; }
    
    var foo = function (argument) { cout << __PRETTY_FUNCTION__ << endl; return argument; };
    
    Document operator + (Document const &) const {} // dummy
    Document operator - (Document const &) const {} // dummy
    Document operator * (Document const &) const {} // dummy
    Document operator / (Document const &) const {} // dummy
};

ostream & operator << (ostream &, Document const &) {} // dummy

int main()
{
    var temporary = new int(1);
    
    Document document;
    document.foo(temporary);
    
    var bar = function ()
    {
        var document = new Document();
        
        return new int(2);
    };
    
    cout << 1 << endl;
    bar();
    cout << 2 << endl;
}
