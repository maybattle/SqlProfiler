using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace HelloWorld
{
    using System.Data.SqlClient;

    public class Persion
    {
        
        public string Name{ get; set; }
        public int Id { get; set; }
        public DateTime DoB { get; set; }
    }

    public partial class Form1 : Form
    {
        Persion p = new Persion();
        private Point point;
        private DateTime dt;
        public Form1()
        {
            InitializeComponent();
            textBox1.Text = "Ticks:"+DateTime.Now.Ticks.ToString();
            var h = new Hellower1();
            ((Hellower1)h).HalloMethod = "Hallo Welt";
            var result = TestDateTime(DateTime.Now);
            TestInt(19);
            TestFloat(float.MaxValue);
            TestDouble(double.MaxValue);
            
            p.DoB = DateTime.Now;
            p.Name = "Ralf Hientzsch-Schindlmeier";
            p.Id = 1;
            p=TestClass(p);
            point = TestPoint();
            TestString("Ralf");
        }

        private double TestDouble(double d)
        {
            return d;
        }


        private Point TestPoint()
        {
            return new Point(1,3);
        }

        private Persion TestClass(Persion p)
        {
            
            Console.WriteLine(p);
            return  new Persion();
        }

        private float TestFloat(float f)
        {
            return f;
        }

        private string TestString(string s)
        {
            return s;
        }


        private void button1_Click(object sender, EventArgs e)
        {
            
            //MessageBox.Show("Hello world!");
            var builder = new SqlConnectionStringBuilder();
            builder.DataSource = "test.sql.id.jwde.local";
            builder.InitialCatalog = "JWConsumerDB";
            builder.IntegratedSecurity = true;
            builder.ApplicationName = "ProfilerApp";

            var conn = new SqlConnection(builder.ToString());
            var cmd = conn.CreateCommand();
            cmd.CommandText = "select * from masterdata.tblActivity where intActivityId = @activityId";
            cmd.Parameters.Add(new SqlParameter("@activityId", 1));
            
            conn.Open();
            var reader = cmd.ExecuteReader(CommandBehavior.CloseConnection);
            while (reader.Read())
            {
                textBox1.Text += reader["strName"].ToString() + "\r\n";
            }
        }

        private DateTime TestDateTime(DateTime a)
        {
            return a;
        }

        private int TestInt(int a)
        {
            return a;
        }
    }

    public class Hellower1 : Hellower
    {
        private string _halliHallo;
        public override string HalloMethod { get { return _halliHallo; } set { _halliHallo = value; } }
    }

    public abstract class Hellower
    {
        private string _halloWelt;

        public abstract string HalloMethod { get; set; }
    }

}